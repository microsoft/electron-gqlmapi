# Electron-GqlMAPI

This is a Node Native Module wrapped around [GqlMAPI](https://github.com/microsoft/gqlmapi), built
against the [Electron](https://www.electronjs.org/) runtime. You could build it for a different
version of Node or v8 as long as it's supported in [CMake.js](https://github.com/cmake-js/cmake-js)
and [NAN](https://github.com/nodejs/nan). The core logic for binding the GqlMAPI service to the
Node v8 runtime is in [NodeBinding.cpp](./src/NodeBinding.cpp), and aside from one extra parameter
in `startService`, it should work with any service generated using
[CppGraphQLGen](https://github.com/microsoft/cppgraphqlgen).

This project was originally based on [electron-cppgraphql](https://github.com/wravery/electron-cppgraphql),
but it has many upgrades to the build process, packaging, and the implementation since then. It is not
cross-platform like electron-cppgraphql (which used a trivial service with mock data), but I intend to feed
these improvements back into electron-cppgraphql. For now, this is a much better starting point if you want
to implement your own Node Native Module for a CppGraphQLGen service.

## Getting Started

To begin, you will need to satisfy the [requirements](https://github.com/microsoft/gqlmapi#getting-started)
for building GqlMAPI. If you are using Vcpkg to install CppGraphQLGen and GoogleTest as suggested in those
instructions, then you will also need to save an npm config setting for CMake.js to use the vcpkg.cmake
toolchain file:

```cmd
> npm config set cmake_CMAKE_TOOLCHAIN_FILE %VCPKG_ROOT%\scripts\buildsystems\vcpkg.cmake
```

Then you can build and test the module using npm:

```cmd
> npm install
> npm test
```

## Sample App

There's a sample integration with [GraphiQL](https://github.com/graphql/graphiql) in
[eMAPI](https://github.com/microsoft/eMAPI). Take a look at
[startElectron.js](https://github.com/microsoft/eMAPI/blob/main/src/startElectron.js) to see how
you can load the native module in the main process and connect to it with Electron's
[IPC](https://www.electronjs.org/docs/api/ipc-main) API from the browser process. The main
[index.js](./lib/index.js) script in this package wraps all of that up in a `startGraphQL`
function.

This project includes a [preload.js](./lib/preload.js) script exposes that IPC channel as a safer
and friendlier API through the
[contextBridge](https://www.electronjs.org/docs/tutorial/context-isolation). After the preload,
Electron blocks access to APIs that haven't been exported over the bridge, including IPC. The path
to the preload script is exported as `preloadPath` from the [index.js](./lib/index.js) script in
this package, so if you `import`/`require` it you can set that directly on the
[BrowserWindow](https://www.electronjs.org/docs/api/browser-window) `webPreferences.preload`
property.

The [index.js](https://github.com/microsoft/eMAPI/blob/main/src/index.js) script loaded in the
browser process puts all of that together in a `fetchQuery` function, which GraphiQL uses for all
service communication. This is specific to the interface that GraphiQL requires, but it is similar
to the way that [Apollo](https://www.apollographql.com/)
[stateless links](https://www.apollographql.com/docs/link/stateless/) work with promises and/or
observables for subscription support. Integrating with another GraphQL client like Apollo or Relay
is currently left as an exercise for the reader, but a future version will probably include
reference implementations, once I have a chance to build test projects for them.

## Contributing

This project welcomes contributions and suggestions.  Most contributions require you to agree to a
Contributor License Agreement (CLA) declaring that you have the right to, and actually do, grant us
the rights to use your contribution. For details, visit https://cla.opensource.microsoft.com.

When you submit a pull request, a CLA bot will automatically determine whether you need to provide
a CLA and decorate the PR appropriately (e.g., status check, comment). Simply follow the instructions
provided by the bot. You will only need to do this once across all repos using our CLA.

This project has adopted the [Microsoft Open Source Code of Conduct](https://opensource.microsoft.com/codeofconduct/).
For more information see the [Code of Conduct FAQ](https://opensource.microsoft.com/codeofconduct/faq/) or
contact [opencode@microsoft.com](mailto:opencode@microsoft.com) with any additional questions or comments.

## Trademarks

This project may contain trademarks or logos for projects, products, or services. Authorized use of Microsoft 
trademarks or logos is subject to and must follow 
[Microsoft's Trademark & Brand Guidelines](https://www.microsoft.com/en-us/legal/intellectualproperty/trademarks/usage/general).
Use of Microsoft trademarks or logos in modified versions of this project must not cause confusion or imply Microsoft sponsorship.
Any use of third-party trademarks or logos are subject to those third-party's policies.
