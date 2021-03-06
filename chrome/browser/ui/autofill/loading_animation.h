// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_UI_AUTOFILL_LOADING_ANIMATION_H_
#define CHROME_BROWSER_UI_AUTOFILL_LOADING_ANIMATION_H_

#include "ui/gfx/animation/linear_animation.h"

namespace autofill {

// An animation for a dancing ellipsis.
class LoadingAnimation : public gfx::LinearAnimation {
 public:
  explicit LoadingAnimation(gfx::AnimationDelegate* delegate);
  virtual ~LoadingAnimation();

  // gfx::Animation implementation.
  virtual void Step(base::TimeTicks time_now) OVERRIDE;

  // Returns the vertical pixel offset for the nth dot.
  double GetCurrentValueForDot(size_t dot_i);

  // Stops this animation. Use this instead of Stop() to make sure future
  // runs don't mess up on the first cycle.
  void Reset();

 private:
  // Describes a frame of the animation, a la -webkit-keyframes.
  struct AnimationFrame {
    double value;
    double position;
  };

  // True if the current cycle is the first one since Reset() was last called.
  bool first_cycle_;
};

}  // namespace autofill

#endif  // CHROME_BROWSER_UI_AUTOFILL_LOADING_ANIMATION_H_
