// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <sys/types.h>
#include <functional>
#include <memory>
#include <utility>
#include "fml/logging.h"
#include "fml/synchronization/waitable_event.h"
#include "gtest/gtest.h"
#include "impeller/renderer/backend/vulkan/resource_manager_vk.h"

namespace impeller {
namespace testing {

// While expected to be a singleton per context, the class does not enforce it.
TEST(ResourceManagerVKTest, CreatesANewInstance) {
  auto const a = ResourceManagerVK::Create();
  auto const b = ResourceManagerVK::Create();
  EXPECT_NE(a, b);
}

namespace {

// Invokes the provided callback when the destructor is called.
//
// Can be moved, but not copied.
class DeathRattle final {
 public:
  explicit DeathRattle(std::function<void()> callback)
      : callback_(std::move(callback)) {}

  DeathRattle(DeathRattle&&) = default;
  DeathRattle& operator=(DeathRattle&&) = default;

  ~DeathRattle() { callback_(); }

 private:
  std::function<void()> callback_;
};

}  // namespace

TEST(ResourceManagerVKTest, ReclaimMovesAResourceAndDestroysIt) {
  auto const manager = ResourceManagerVK::Create();

  auto waiter = fml::AutoResetWaitableEvent();
  auto dead = false;
  auto rattle = DeathRattle([&waiter]() { waiter.Signal(); });

  // Not killed immediately.
  EXPECT_FALSE(waiter.IsSignaledForTest());

  {
    auto resource = UniqueResourceVKT<DeathRattle>(manager, std::move(rattle));
  }

  waiter.Wait();
}

// Regression test for https://github.com/flutter/flutter/issues/134482.
TEST(ResourceManagerVKTest, TerminatesWhenOutOfScope) {
  // Originally, this shared_ptr was never destroyed, and the thread never
  // terminated. This test ensures that the thread terminates when the
  // ResourceManagerVK is out of scope.
  std::weak_ptr<ResourceManagerVK> manager;

  {
    auto shared = ResourceManagerVK::Create();
    manager = shared;
  }

  // The thread should have terminated.
  EXPECT_EQ(manager.lock(), nullptr);
}

}  // namespace testing
}  // namespace impeller
