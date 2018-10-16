/**
 * @file main.cc
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

#include "../include/inotify.h"
// testing
void MyCallBack(inotify::notification notif, std::string file_name) {
  if (notif & inotify::notification::kAccess)
    std::cout << "\n\n\nfile: " << file_name << " accessed\n\n" << std::endl;
  if (notif & inotify::notification::kModify)
    std::cout << "\n\n\nfile: " << file_name << " modified\n\n" << std::endl;
}

int main(int argc, char const *argv[]) {
  inotify::EventListener u;
  u.Listen("/tmp", inotify::notification::kModify, MyCallBack);

  // listen to notifications for 1 minute
  std::this_thread::sleep_for(std::chrono::minutes(1));
}
