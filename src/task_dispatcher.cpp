#include "task_dispatcher.hpp"

#include "command.hpp"
#include "task_handler.hpp"

namespace MEngine {

TaskDispatcher::TaskDispatcher() {}

TaskDispatcher::~TaskDispatcher() {}

void TaskDispatcher::PushHandler(std::shared_ptr<TaskHandler> task_handler) {
  task_handlers_.push_back(task_handler);
}

void TaskDispatcher::Run(Command& cmd) {
  for (auto& task_handler : task_handlers_) {
    if (cmd.IsCancelled()) {
      break;
    }
    task_handler->Run(cmd);
  }
}

}  // namespace MEngine