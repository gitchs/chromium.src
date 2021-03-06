// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
#define CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_

#include "base/compiler_specific.h"
#include "third_party/WebKit/public/platform/WebServiceWorker.h"
#include "third_party/WebKit/public/web/WebFrame.h"

namespace content {

class WebServiceWorkerImpl
    : NON_EXPORTED_BASE(public WebKit::WebServiceWorker) {
 public:
  explicit WebServiceWorkerImpl(int64 service_worker_id)
      : service_worker_id_(service_worker_id) {}
  virtual ~WebServiceWorkerImpl();

 private:
  int64 service_worker_id_;

  DISALLOW_COPY_AND_ASSIGN(WebServiceWorkerImpl);
};

}  // namespace content

#endif  // CONTENT_CHILD_SERVICE_WORKER_WEB_SERVICE_WORKER_IMPL_H_
