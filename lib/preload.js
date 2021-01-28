const { ipcRenderer, contextBridge } = require("electron");

let _callbacks = [];

ipcRenderer.on("fetched", (_event, queryId, payload) => {
  const result = JSON.parse(payload);
  _callbacks
    .filter((callback) => callback.queryId === queryId)
    .forEach((callback) => callback.next(result));
});

ipcRenderer.on("completed", (_event, queryId) => {
  _callbacks
    .filter((callback) => callback.queryId === queryId)
    .forEach((callback) => callback.complete());

  _callbacks = _callbacks.filter((callback) => callback.queryId !== queryId);
});

contextBridge.exposeInMainWorld("gqlmapi", {
  startService: () => ipcRenderer.invoke("startService"),
  stopService: () => ipcRenderer.invoke("stopService"),
  parseQuery: (query) => ipcRenderer.invoke("parseQuery", query),
  discardQuery: (queryId) => ipcRenderer.invoke("discardQuery", queryId),
  fetchQuery: (queryId, operationName, variables, next, complete) => {
    _callbacks.push({ queryId, next, complete });
    ipcRenderer.send("fetchQuery", queryId, operationName, variables);
  },
  unsubscribe: (queryId) => ipcRenderer.invoke("unsubscribe", queryId),
});
