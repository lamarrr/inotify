/**
 * @file inotify.cc
 * @author lamar (rlamarrr@gmail.com)
 * @brief
 * @version 0.1
 * @date 2018-10-16
 * 
 * @copyright Copyright (c) 2018

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at
    http://www.apache.org/licenses/LICENSE-2.0
Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
==============================================================================
*/

#include <sys/inotify.h>

#include <unistd.h>

#include <algorithm>
#include <exception>
#include <iostream>
#include <thread>  //NOLINT
#include <tuple>
#include <vector>

#ifdef NOTIF_DEBUG_MODE
#define LOG(message) std::clog << "[Log] " << message << std::endl
#else
#define LOG(message) 0
#endif

namespace inotify {

enum notification : uint {
  kCreate = IN_CREATE,
  kDelete = IN_DELETE,
  kAccess = IN_ACCESS,
  kClose = IN_CLOSE,
  kModify = IN_MODIFY,
  kMove = IN_MOVE,
  kOpen = IN_OPEN,
  kAll = IN_ALL_EVENTS
};

constexpr size_t kEventSize = sizeof(inotify_event);
constexpr size_t kBufferLength = 1024 * (kEventSize + 16);
constexpr size_t kBufferSize = kBufferLength / sizeof(char);

using EventCallBack = void (*)(notification, std::string);

class Error {
  const char *err_details_;

 public:
  explicit Error(const char *error) : err_details_(error) {}
  const char *what() noexcept { return err_details_; }
};

class SystemError : public Error {
 public:
  explicit SystemError(const char *error_str) : Error(error_str) {}
};

class EventListener {
  typedef int FileDescriptor;
  typedef int WatchDescriptor;

  std::vector<std::tuple<FileDescriptor, WatchDescriptor, notification,
                         std::thread *, EventCallBack>>
      events_handle_;

  /**
   * @brief
   *
   * @param callback
   * @param notif
   * @param file_desc
   * @param watch_desc
   */
  static void ThreadLoop_(EventCallBack callback, notification notif,
                          FileDescriptor file_desc,
                          WatchDescriptor watch_desc) {
    std::allocator<char> allocator_;
    while (true) {
      // contains multiple events at a single read
      // handles errors and sudden closure
      std::unique_ptr<char> events_buffer_handle(
          allocator_.allocate(kBufferSize));
      char *events_buffer_ptr = events_buffer_handle.get();
      int len = read(file_desc, events_buffer_ptr, kBufferSize);

      int current_event_buffer_index = 0;

      if (len == 0) {
        std::clog << "buffer allocated too small" << std::endl;
      }

      while (current_event_buffer_index < len) {
        inotify_event *event = reinterpret_cast<inotify_event *>(
            &(events_buffer_ptr[current_event_buffer_index]));

        if (IsMatchingNotification(notif, event->mask)) {
          // user decides what to do with file name
          callback(notif, std::string{event->name});
        }

        // increase by byte index as offset for next iteration
        size_t extra_alloc = event->len;
        size_t used_buffer_size = kEventSize + extra_alloc;
        current_event_buffer_index += (used_buffer_size / sizeof(char));
      }
      if (errno == EINTR) {
        std::clog << "need to reissue sys call";
      }
    }
  }

  static inline bool IsMatchingNotification(notification notif,
                                            uint32_t event_mask) {
    return notif & event_mask;
  }
  static inline const FileDescriptor InitFileDescriptor_() {
    return inotify_init();
  }
  static inline const WatchDescriptor InitWatchDescriptor_(
      const char *path, FileDescriptor file_desc, notification notif) {
    return inotify_add_watch(file_desc, path, notif);
  }

 public:
  void Listen(const char *path, notification notif, EventCallBack callback) {
    const FileDescriptor file_desc = InitFileDescriptor_();
    if (file_desc < 0) throw SystemError("inotify_init error");

    // callback(notification::kModify, "a.cc");
    const WatchDescriptor watch_desc =
        InitWatchDescriptor_(path, file_desc, notif);
    std::thread *notif_thread_handle =
        new std::thread(ThreadLoop_, callback, notif, file_desc, watch_desc);

    events_handle_.emplace_back(file_desc, watch_desc, notif,
                                notif_thread_handle, callback);
    // init and launch thread for listening
    // get new event listener to notification type
  }

  void StopAll() {
    using Iterator = std::vector<FileDescriptor>::iterator;
    std::vector<FileDescriptor> file_descs;
    for (auto &[file_desc, watch_desc, notif, thread, callback] :
         events_handle_) {
      if (thread->joinable()) thread->detach();
      thread->~thread();

      inotify_rm_watch(watch_desc, file_desc);
      file_descs.push_back(file_desc);
    }

    std::unique(file_descs.begin(), file_descs.end());

    for (FileDescriptor fd : file_descs) {
      int sys_return = close(fd);
      if (sys_return != 0) {
        std::clog << "error occured while closing file descriptor" << std::endl;
        // throw SystemError("error occured while closing file descriptor");
      }
    }

    events_handle_.clear();
  }

  ~EventListener() noexcept { this->StopAll(); }
};

};  // namespace inotify
