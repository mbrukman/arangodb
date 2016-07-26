////////////////////////////////////////////////////////////////////////////////
/// DISCLAIMER
///
/// Copyright 2014-2016 ArangoDB GmbH, Cologne, Germany
/// Copyright 2004-2014 triAGENS GmbH, Cologne, Germany
///
/// Licensed under the Apache License, Version 2.0 (the "License");
/// you may not use this file except in compliance with the License.
/// You may obtain a copy of the License at
///
///     http://www.apache.org/licenses/LICENSE-2.0
///
/// Unless required by applicable law or agreed to in writing, software
/// distributed under the License is distributed on an "AS IS" BASIS,
/// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
/// See the License for the specific language governing permissions and
/// limitations under the License.
///
/// Copyright holder is ArangoDB GmbH, Cologne, Germany
///
/// @author Dr. Frank Celler
/// @author Achim Brandt
////////////////////////////////////////////////////////////////////////////////

#ifndef ARANGOD_SCHEDULER_TASK_H
#define ARANGOD_SCHEDULER_TASK_H 1

#include "Basics/Common.h"

#include "Scheduler/events.h"
#include "lib/Rest/HttpResponse.h"
#include "Statistics/StatisticsAgent.h"

namespace arangodb {
namespace velocypack {
class Builder;
}

namespace rest {
class Scheduler;

class TaskData : public RequestStatisticsAgent {
 public:
  static uint64_t const TASK_DATA_RESPONSE = 1000;
  static uint64_t const TASK_DATA_CHUNK    = 1001;
  static uint64_t const TASK_DATA_BUFFER   = 1002;

 public:
  uint64_t _taskId;
  EventLoop _loop;
  uint64_t _type;
  std::string _data;
  std::unique_ptr<GeneralResponse> _response;
  std::shared_ptr<velocypack::Buffer<uint8_t>> _buffer;
};

////////////////////////////////////////////////////////////////////////////////
/// @brief abstract base class for tasks
////////////////////////////////////////////////////////////////////////////////

class Task {
  Task(Task const&) = delete;
  Task& operator=(Task const&) = delete;

  friend class TaskManager;

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new task
  ///
  /// Note that the constructor has no access to the event loop. The connection
  /// is provided in the method setup and any setup with regards to the event
  /// loop must be done there. It is not possible to simply delete a tasks. You
  /// must use the method destroy to cleanup the task, remove it from the event
  /// loop and delete it. The method cleanup itself will not delete task but
  /// remove it from the event loop. It is possible to use setup again to reuse
  /// the task.
  //////////////////////////////////////////////////////////////////////////////

  Task(std::string const& id, std::string const& name);

  //////////////////////////////////////////////////////////////////////////////
  /// @brief constructs a new task
  //////////////////////////////////////////////////////////////////////////////

  explicit Task(std::string const& name);

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief deletes a task
  ///
  /// The method will only be called from after the task has been cleaned by
  /// the method cleanup.
  //////////////////////////////////////////////////////////////////////////////

  virtual ~Task();

 public:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the task name for debugging
  //////////////////////////////////////////////////////////////////////////////

  std::string const& name() const { return _name; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the task id
  //////////////////////////////////////////////////////////////////////////////

  std::string id() const { return _id; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the internal event loop
  //////////////////////////////////////////////////////////////////////////////

  EventLoop eventLoop() const { return _loop; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief returns the internal task identifier
  //////////////////////////////////////////////////////////////////////////////

  uint64_t taskId() const { return _taskId; }

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a VelocyPack representation of the task
  //////////////////////////////////////////////////////////////////////////////

  std::shared_ptr<arangodb::velocypack::Builder> toVelocyPack() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a VelocyPack representation of the task
  //////////////////////////////////////////////////////////////////////////////

  void toVelocyPack(arangodb::velocypack::Builder&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief whether or not the task is a user task
  //////////////////////////////////////////////////////////////////////////////

  virtual bool isUserDefined() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief allow thread to run on slave event loop
  //////////////////////////////////////////////////////////////////////////////

  virtual bool needsMainEventLoop() const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called by scheduler to indicate an event
  ///
  /// The method will only be called from within the scheduler thread, which
  /// belongs to the loop parameter.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool handleEvent(EventToken token, EventType event) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief signals a result
  //////////////////////////////////////////////////////////////////////////////

  virtual void signalTask(TaskData*) {}

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief get a task specific description in JSON format
  //////////////////////////////////////////////////////////////////////////////

  virtual void getDescription(arangodb::velocypack::Builder&) const;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called to set up the callback information
  ///
  /// The method will only be called from within the scheduler thread, which
  /// belongs to the loop parameter.
  //////////////////////////////////////////////////////////////////////////////

  virtual bool setup(Scheduler*, EventLoop) = 0;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief called to clear the callback information
  ///
  /// The method will only be called from within the scheduler thread, which
  /// belongs to the loop parameter.
  //////////////////////////////////////////////////////////////////////////////

  virtual void cleanup() = 0;

 protected:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief scheduler
  //////////////////////////////////////////////////////////////////////////////

  Scheduler* _scheduler;  // TODO (fc) XXX make that a singleton

  //////////////////////////////////////////////////////////////////////////////
  /// @brief task id
  //////////////////////////////////////////////////////////////////////////////

  uint64_t const _taskId;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief event loop identifier
  //////////////////////////////////////////////////////////////////////////////

  EventLoop _loop;

 private:
  //////////////////////////////////////////////////////////////////////////////
  /// @brief task id
  //////////////////////////////////////////////////////////////////////////////

  std::string const _id;

  //////////////////////////////////////////////////////////////////////////////
  /// @brief task name
  //////////////////////////////////////////////////////////////////////////////

  std::string const _name;
};
}
}

#endif
