// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

#include "graphqlservice/JSONResponse.h"

#include "MAPIGraphQL.h"

#include <nan.h>

#include <iostream>
#include <memory>
#include <map>
#include <thread>
#include <queue>

using v8::Function;
using v8::FunctionTemplate;
using v8::Promise;
using v8::Local;
using v8::String;
using v8::Int32;
using v8::Value;
using Nan::AsyncQueueWorker;
using Nan::AsyncProgressQueueWorker;
using Nan::Callback;
using Nan::DecodeWrite;
using Nan::Encoding;
using Nan::GetFunction;
using Nan::HandleScope;
using Nan::New;
using Nan::Null;
using Nan::Set;
using Nan::To;

using namespace graphql;

namespace {

std::shared_ptr<service::Request> serviceSingleton;

} // namespace

NAN_METHOD(StartService)
{
	const auto useDefaultProfile = To<bool>(info[0]).FromMaybe(false);

	serviceSingleton = mapi::GetService(useDefaultProfile);
}

struct SubscriptionPayloadQueue : std::enable_shared_from_this<SubscriptionPayloadQueue>
{
	~SubscriptionPayloadQueue()
	{
		Unsubscribe();
	}

	void Unsubscribe()
	{
		std::unique_lock<std::mutex> lock(mutex);

		if (!registered)
		{
			return;
		}

		registered = false;

		auto deferUnsubscribe = std::move(key);

		lock.unlock();
		condition.notify_one();

		if (deferUnsubscribe
			&& serviceSingleton)
		{
			serviceSingleton->unsubscribe(std::launch::deferred, *deferUnsubscribe).get();
		}
	}

	std::mutex mutex;
	std::condition_variable condition;
	std::queue<std::future<response::Value>> payloads;
	std::optional<service::SubscriptionKey> key;
	bool registered = false;
};

static std::map<std::int32_t, peg::ast> queryMap;
static std::map<std::int32_t, std::shared_ptr<SubscriptionPayloadQueue>> subscriptionMap;

NAN_METHOD(StopService)
{
	if (serviceSingleton)
	{
		for (const auto& entry : subscriptionMap)
		{
			entry.second->Unsubscribe();
		}

		subscriptionMap.clear();
		queryMap.clear();
		serviceSingleton.reset();
	}
}

NAN_METHOD(ParseQuery)
{
	std::string query(*Nan::Utf8String(To<String>(info[0]).ToLocalChecked()));
	const std::int32_t queryId = (queryMap.empty() ? 1 : queryMap.crbegin()->first + 1);

	try
	{
		queryMap[queryId] = peg::parseString(query);
		info.GetReturnValue().Set(New<Int32>(queryId));
	}
	catch (const std::exception& ex)
	{
		Nan::ThrowError(ex.what());
	}
}

NAN_METHOD(DiscardQuery)
{
	const auto queryId = To<std::int32_t>(info[0]).FromJust();

	queryMap.erase(queryId);
}

class RegisteredSubscription : public AsyncProgressQueueWorker<std::string>
{
public:
	explicit RegisteredSubscription(std::int32_t queryId, std::string&& operationName, const std::string& variables, std::unique_ptr<Callback>&& next, std::unique_ptr<Callback>&& complete)
		: AsyncProgressQueueWorker(complete.release(), "graphql:subscription")
		, _next{ std::move(next) }
	{
		try
		{
			_payloadQueue = std::make_shared<SubscriptionPayloadQueue>();

			const auto itrQuery = queryMap.find(queryId);

			if (itrQuery == queryMap.cend())
			{
				throw std::runtime_error("Unknown queryId");
			}

			auto& ast = itrQuery->second;
			auto parsedVariables = (variables.empty() ? response::Value(response::Type::Map) : response::parseJSON(variables));

			if (parsedVariables.type() != response::Type::Map)
			{
				throw std::runtime_error("Invalid variables object");
			}

			std::unique_lock<std::mutex> lock(_payloadQueue->mutex);

			if (serviceSingleton->findOperationDefinition(ast, operationName).first == service::strSubscription)
			{
				_payloadQueue->registered = true;
				_payloadQueue->key = std::make_optional(serviceSingleton->subscribe(std::launch::deferred, service::SubscriptionParams{
					nullptr,
					peg::ast{ ast },
					std::move(operationName),
					std::move(parsedVariables)
					}, [spQueue = _payloadQueue](std::future<response::Value> payload) noexcept -> void
				{
					std::unique_lock<std::mutex> lock(spQueue->mutex);

					if (!spQueue->registered)
					{
						return;
					}

					spQueue->payloads.push(std::move(payload));

					lock.unlock();
					spQueue->condition.notify_one();
				}).get());
			}
			else
			{
				_payloadQueue->payloads.push(serviceSingleton->resolve(
					std::launch::deferred,
					nullptr,
					ast,
					operationName,
					std::move(parsedVariables)));

				lock.unlock();
				_payloadQueue->condition.notify_one();
			}
		}
		catch (const std::exception& ex)
		{
			std::cerr << "Caught exception preparing the subscription: " << ex.what() << std::endl;
		}
	}

	~RegisteredSubscription()
	{
		if (_payloadQueue)
		{
			_payloadQueue->Unsubscribe();
		}
	}

	const std::shared_ptr<SubscriptionPayloadQueue>& GetPayloadQueue() const
	{
		return _payloadQueue;
	}

private:
	// Executed inside the worker-thread.
	// It is not safe to access V8, or V8 data structures
	// here, so everything we need for input and output
	// should go on `this`.
	void Execute(const ExecutionProgress& progress) override
	{
		auto spQueue = _payloadQueue;
		bool registered = true;

		while (registered)
		{
			std::unique_lock<std::mutex> lock(spQueue->mutex);

			spQueue->condition.wait(lock, [spQueue]() noexcept -> bool
			{
				return !spQueue->registered
					|| !spQueue->payloads.empty();
			});

			auto payloads = std::move(spQueue->payloads);

			registered = spQueue->registered;
			lock.unlock();

			std::vector<std::string> json;

			while (!payloads.empty())
			{
				response::Value document{ response::Type::Map };
				auto payload = std::move(payloads.front());

				payloads.pop();

				try
				{
					document = payload.get();
				}
				catch (service::schema_exception& scx)
				{
					document.reserve(2);
					document.emplace_back(std::string{ service::strData }, {});
					document.emplace_back(std::string{ service::strErrors }, scx.getErrors());
				}
				catch (const std::exception& ex)
				{
					std::ostringstream oss;

					oss << "Caught exception delivering subscription payload: " << ex.what();
					document.reserve(2);
					document.emplace_back(std::string{ service::strData }, {});
					document.emplace_back(std::string{ service::strErrors }, response::Value{ oss.str() });
				}

				json.push_back(response::toJSON(std::move(document)));
			}

			if (!json.empty())
			{
				progress.Send(json.data(), json.size());
			}
		}
	}

	// Executed when the async results are ready
	// this function will be run inside the main event loop
	// so it is safe to use V8 again
	void HandleProgressCallback(const std::string* data, size_t size) override
	{
		if (data == nullptr)
		{
			return;
		}

		HandleScope scope;

		while (size-- > 0)
		{
			Local<Value> argv[] = {
				New<String>(data->c_str(), static_cast<int>(data->size())).ToLocalChecked()
			};

			_next->Call(1, argv, async_resource);
			++data;
		}
	}

	std::unique_ptr<Callback> _next;
	std::shared_ptr<SubscriptionPayloadQueue> _payloadQueue;
};

NAN_METHOD(FetchQuery)
{
	const auto queryId = To<std::int32_t>(info[0]).FromJust();
	std::string operationName(*Nan::Utf8String(To<String>(info[1]).ToLocalChecked()));
	std::string variables(*Nan::Utf8String(To<String>(info[2]).ToLocalChecked()));
	auto next = std::make_unique<Callback>(To<Function>(info[3]).ToLocalChecked());
	auto complete = std::make_unique<Callback>(To<Function>(info[4]).ToLocalChecked());
	auto subscription = std::make_unique<RegisteredSubscription>(queryId, std::move(operationName), variables, std::move(next), std::move(complete));

	subscriptionMap[queryId] = subscription->GetPayloadQueue();
	AsyncQueueWorker(subscription.release());
}

NAN_METHOD(Unsubscribe)
{
	const auto queryId = To<std::int32_t>(info[0]).FromJust();
	auto itr = subscriptionMap.find(queryId);

	if (itr != subscriptionMap.end())
	{
		itr->second->Unsubscribe();
		subscriptionMap.erase(itr);
	}
}

NAN_MODULE_INIT(Init)
{
	Set(target, New<String>("startService").ToLocalChecked(),
		GetFunction(New<FunctionTemplate>(StartService)).ToLocalChecked());

	Set(target, New<String>("stopService").ToLocalChecked(),
		GetFunction(New<FunctionTemplate>(StopService)).ToLocalChecked());

	Set(target, New<String>("parseQuery").ToLocalChecked(),
		GetFunction(New<FunctionTemplate>(ParseQuery)).ToLocalChecked());

	Set(target, New<String>("discardQuery").ToLocalChecked(),
		GetFunction(New<FunctionTemplate>(DiscardQuery)).ToLocalChecked());

	Set(target, New<String>("fetchQuery").ToLocalChecked(),
		GetFunction(New<FunctionTemplate>(FetchQuery)).ToLocalChecked());

	Set(target, New<String>("unsubscribe").ToLocalChecked(),
		GetFunction(New<FunctionTemplate>(Unsubscribe)).ToLocalChecked());
}

NODE_MODULE(gqlmapi, Init)