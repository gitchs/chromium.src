// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_H_
#define CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_H_

#include <string>

#include "base/callback.h"
#include "base/memory/scoped_ptr.h"
#include "content/browser/frame_host/frame_tree_node.h"
#include "content/common/content_export.h"

namespace content {

class FrameTreeNode;
class RenderProcessHost;
class RenderViewHostImpl;

// Represents the frame tree for a page. With the exception of the main frame,
// all FrameTreeNodes will be created/deleted in response to frame attach and
// detach events in the DOM.
//
// The main frame's FrameTreeNode is special in that it is reused. This allows
// it to serve as an anchor for state that needs to persist across top-level
// page navigations.
//
// TODO(ajwong): Move NavigationController ownership to the main frame
// FrameTreeNode. Possibly expose access to it from here.
//
// TODO(ajwong): Currently this class only contains FrameTreeNodes for
// subframes if the --site-per-process flag is enabled.
//
// This object is only used on the UI thread.
class CONTENT_EXPORT FrameTree {
 public:
  FrameTree();
  ~FrameTree();

  // Returns the FrameTreeNode with the given |frame_id|.
  FrameTreeNode* FindByID(int64 frame_id);

  // Executes |on_node| on each node in the frame tree.  If |on_node| returns
  // false, terminates the iteration immediately. Returning false is useful
  // if |on_node| is just doing a search over the tree.
  void ForEach(const base::Callback<bool(FrameTreeNode*)>& on_node) const;

  // After the FrameTree is created, or after SwapMainFrame() has been called,
  // the root node does not yet have a frame id. This is allocated by the
  // renderer and is published to the browser process on the first navigation
  // after a swap. These two functions are used to set the root node's frame
  // id.
  //
  // TODO(ajwong): Remove these once RenderFrameHost's routing id replaces
  // frame_id.
  bool IsFirstNavigationAfterSwap() const;
  void OnFirstNavigationAfterSwap(int main_frame_id);

  // Frame tree manipulation routines.
  void AddFrame(int render_frame_host_id, int64 parent_frame_id,
                int64 frame_id, const std::string& frame_name);
  void RemoveFrame(int64 parent_frame_id, int64 frame_id);
  void SetFrameUrl(int64 frame_id, const GURL& url);

  // Resets the FrameTree and changes RenderFrameHost for the main frame.
  // This destroys most of the frame tree but retains the root node so that
  // navigation state may be kept on it between process swaps. Used to
  // support bookkeeping for top-level navigations.
  //
  // If |main_frame| is NULL, reset tree to initially constructed state.
  //
  // TODO(ajwong): This function should not be given a |main_frame|. This is
  // required currently because the RenderViewHost owns its main frame. When
  // that relation is fixed, the FrameTree should be responsible for
  // created/destroying the main frame on the swap.
  void SwapMainFrame(RenderFrameHostImpl* main_frame);

  // Convenience accessor for the main frame's RenderFrameHostImpl.
  RenderFrameHostImpl* GetMainFrame() const;

  // Allows a client to listen for frame removal.
  void SetFrameRemoveListener(
      const base::Callback<void(RenderViewHostImpl*, int64)>& on_frame_removed);

  FrameTreeNode* GetRootForTesting() { return root_.get(); }

 private:
  scoped_ptr<FrameTreeNode> CreateNode(int64 frame_id,
                                       const std::string& frame_name,
                                       int render_frame_host_id,
                                       RenderProcessHost* render_process_host);

  scoped_ptr<FrameTreeNode> root_;

  base::Callback<void(RenderViewHostImpl*, int64)> on_frame_removed_;

  DISALLOW_COPY_AND_ASSIGN(FrameTree);
};

}  // namespace content

#endif  // CONTENT_BROWSER_FRAME_HOST_FRAME_TREE_H_
