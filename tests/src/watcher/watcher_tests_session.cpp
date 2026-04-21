// Reason: Keep watcher session open/close and one-shot polling regressions together because they verify the live session boundary.

#include "watcher/watcher_test_suites.h"

#include "watcher/session.h"
#include "watcher/watcher.h"
#include "watcher/watcher_test_support.h"
#include "support/test_support.h"

#include <Windows.h>

#include <filesystem>
#include <vector>

namespace {

using kernel::watcher::CoalescedAction;
using kernel::watcher::CoalescedActionKind;
using watcher_tests::make_temp_watch_root;

void test_watch_session_open_and_close() {
  const auto root = make_temp_watch_root();
  kernel::watcher::WatchSession session;
  const std::error_code ec = kernel::watcher::open_watch_session(root, session);
  require_true(!ec, "watch session should open");
  require_true(session.directory_handle != INVALID_HANDLE_VALUE, "watch session should own a directory handle");
  require_true(session.event_handle != nullptr, "watch session should own an event handle");
  kernel::watcher::close_watch_session(session);
  require_true(session.directory_handle == INVALID_HANDLE_VALUE, "close should release directory handle");
  require_true(session.event_handle == nullptr, "close should release event handle");
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_create_as_refresh() {
  const auto root = make_temp_watch_root();
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  write_file_bytes(root / "created.md", "created");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "create poll should succeed");
  require_true(actions.size() == 1, "create poll should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "create poll should coalesce to refresh");
  require_true(actions[0].rel_path == "created.md", "create poll should preserve path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_injected_overflow_as_full_rescan() {
  const auto root = make_temp_watch_root();
  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  kernel::watcher::inject_next_poll_overflow(session);

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "injected overflow poll should succeed");
  require_true(actions.size() == 1, "injected overflow poll should yield one action");
  require_true(
      actions[0].kind == CoalescedActionKind::FullRescan,
      "injected overflow poll should surface a full-rescan action");
  require_true(actions[0].rel_path.empty(), "injected overflow full-rescan should not carry a path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_modify_as_refresh() {
  const auto root = make_temp_watch_root();
  write_file_bytes(root / "modified.md", "before");

  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  write_file_bytes(root / "modified.md", "after");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "modify poll should succeed");
  require_true(actions.size() == 1, "modify poll should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::RefreshPath, "modify poll should coalesce to refresh");
  require_true(actions[0].rel_path == "modified.md", "modify poll should preserve path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

void test_watch_session_poll_once_reports_delete_as_delete() {
  const auto root = make_temp_watch_root();
  write_file_bytes(root / "deleted.md", "before-delete");

  kernel::watcher::WatchSession session;
  require_true(!kernel::watcher::open_watch_session(root, session), "watch session should open");

  std::filesystem::remove(root / "deleted.md");

  std::vector<CoalescedAction> actions;
  const std::error_code ec = kernel::watcher::poll_watch_session_once(session, 1000, actions);
  require_true(!ec, "delete poll should succeed");
  require_true(actions.size() == 1, "delete poll should yield one action");
  require_true(actions[0].kind == CoalescedActionKind::DeletePath, "delete poll should coalesce to delete");
  require_true(actions[0].rel_path == "deleted.md", "delete poll should preserve path");

  kernel::watcher::close_watch_session(session);
  std::filesystem::remove_all(root);
}

}  // namespace

void run_watcher_session_tests() {
  test_watch_session_open_and_close();
  test_watch_session_poll_once_reports_create_as_refresh();
  test_watch_session_poll_once_reports_injected_overflow_as_full_rescan();
  test_watch_session_poll_once_reports_modify_as_refresh();
  test_watch_session_poll_once_reports_delete_as_delete();
}
