// Copyright (c) Microsoft Corporation. All rights reserved.
// Licensed under the MIT License.

describe("GraphQL native module tests", () => {
  const graphql = require("../build/Debug/electron-gqlmapi");

  it("starts the service", () => {
    graphql.startService(true);
  });

  let queryId = null;

  it("parses the introspection query", () => {
    queryId = graphql.parseQuery(`query {
            __schema {
                queryType {
                    name
                }
                mutationType {
                    name
                }
                subscriptionType {
                    name
                }
                types {
                    kind
                    name
                }
            }
        }`);
    expect(queryId).not.toBeNull();
  });

  it("fetches introspection", async () => {
    expect(queryId).not.toBeNull();
    return expect(
      new Promise((resolve) => {
        let result = null;
        graphql.fetchQuery(
          queryId,
          "",
          "",
          (payload) => {
            result = JSON.parse(payload);
          },
          () => {
            resolve(result);
          }
        );
      })
    ).resolves.toMatchSnapshot();
  });

  it("cleans up after the query", () => {
    expect(queryId).not.toBeNull();
    graphql.unsubscribe(queryId);
    graphql.discardQuery(queryId);
    queryId = null;
  });

  it("stops the service", () => {
    expect(queryId).toBeNull();
    graphql.stopService();
  });
});
