// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_WORKER_WEBWORKERCLIENT_PROXY_H_
#define CONTENT_WORKER_WEBWORKERCLIENT_PROXY_H_

#include "base/basictypes.h"
#include "base/memory/weak_ptr.h"
#include "ipc/ipc_channel.h"
#include "third_party/WebKit/public/web/WebSharedWorkerClient.h"

namespace WebKit {
class WebApplicationCacheHost;
class WebApplicationCacheHostClient;
class WebFrame;
class WebSecurityOrigin;
}

namespace content {

class SharedWorkerDevToolsAgent;
class WebSharedWorkerStub;

// This class receives IPCs from the renderer and calls the WebCore::Worker
// implementation (after the data types have been converted by glue code).  It
// is also called by the worker code and converts these function calls into
// IPCs that are sent to the renderer, where they're converted back to function
// calls by WebWorkerProxy.
class WebSharedWorkerClientProxy : public WebKit::WebSharedWorkerClient {
 public:
  WebSharedWorkerClientProxy(int route_id, WebSharedWorkerStub* stub);
  virtual ~WebSharedWorkerClientProxy();

  // WebSharedWorkerClient implementation.
  virtual void workerContextClosed();
  virtual void workerContextDestroyed();

  virtual WebKit::WebNotificationPresenter* notificationPresenter();

  virtual WebKit::WebApplicationCacheHost* createApplicationCacheHost(
      WebKit::WebApplicationCacheHostClient* client);
  virtual WebKit::WebWorkerPermissionClientProxy*
      createWorkerPermissionClientProxy(
          const WebKit::WebSecurityOrigin& origin);

  // TODO(kinuko): Deprecate these methods.
  virtual bool allowDatabase(WebKit::WebFrame* frame,
                             const WebKit::WebString& name,
                             const WebKit::WebString& display_name,
                             unsigned long estimated_size);
  virtual bool allowFileSystem();
  virtual bool allowIndexedDB(const WebKit::WebString&);

  virtual void dispatchDevToolsMessage(const WebKit::WebString&);
  virtual void saveDevToolsAgentState(const WebKit::WebString&);

  void EnsureWorkerContextTerminates();

  void set_devtools_agent(SharedWorkerDevToolsAgent* devtools_agent) {
    devtools_agent_ = devtools_agent;
  }

 private:
  bool Send(IPC::Message* message);

  int route_id_;
  int appcache_host_id_;
  WebSharedWorkerStub* stub_;
  base::WeakPtrFactory<WebSharedWorkerClientProxy> weak_factory_;
  SharedWorkerDevToolsAgent* devtools_agent_;

  DISALLOW_COPY_AND_ASSIGN(WebSharedWorkerClientProxy);
};

}  // namespace content

#endif  // CONTENT_WORKER_WEBWORKERCLIENT_PROXY_H_
