/*
 * Copyright (c) 2013 Teragon Audio. All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *
 * - Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 * - Redistributions in binary form must reproduce the above copyright notice,
 *   this list of conditions and the following disclaimer in the documentation
 *   and/or other materials provided with the distribution.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef __PluginParameterSet_h__
#define __PluginParameterSet_h__

#include <map>
#include <vector>
#include "PluginParameter.h"
#if ENABLE_MULTITHREADED
#include "EventDispatcher.h"
#endif

namespace teragon {

#if ENABLE_MULTITHREADED
static void asyncDispatcherCallback(void *arg) {
  EventDispatcher* dispatcher = reinterpret_cast<EventDispatcher*>(arg);
  while(!dispatcher->isKilled()) {
    dispatcher->wait();
    dispatcher->process();
  }
}
#endif

class PluginParameterSet
#if ENABLE_MULTITHREADED
  : public EventScheduler
#endif
{
public:
  explicit PluginParameterSet()
#if ENABLE_MULTITHREADED
  : asyncDispatcher(this, false),
    realtimeDispatcher(this, true),
    asyncDispatcherThread(asyncDispatcherCallback, &asyncDispatcher)
#endif
  {
#if ENABLE_MULTITHREADED
    asyncDispatcherThread.detach();
#endif
  }

  virtual ~PluginParameterSet() {
    asyncDispatcherThread.join();

    // Delete all parameters added to the set
    for(int i = 0; i < size(); i++) {
      delete parameterList.at(i);
    }
  }

  /**
   * Add a parameter to the set. Note that this class does *not* free the
   * parameter's memory upon destruction, if this is important to you then
   * you must call the clear() method yourself before destroying this set
   * instance.
   *
   * @param parameter Pointer to parameter instance
   * @return parameter which was added if successful, NULL otherwise. Note that
   *         adding a parameter to a set twice is considered failing behavior.
   */
  virtual PluginParameter* add(PluginParameter* parameter) {
    if(parameter == NULL || get(parameter->getName()) != NULL) {
      return NULL;
    }
    parameterMap.insert(std::make_pair(parameter->getSafeName(), parameter));
    parameterList.push_back(parameter);
    return parameter;
  }

  /**
   * @return Number of parameters in the set
   */
  virtual const int size() const { return parameterList.size(); }


  virtual void clear() {
    for(ParameterList::iterator iterator = parameterList.begin(); iterator != parameterList.end(); ++iterator) {
      delete *iterator;
    }
    parameterList.clear();
    parameterMap.clear();
  }

  /**
   * Lookup a parameter by index, for example: parameterSet[2]
   *
   * @param i Parameter index, must be less than the set's size or undefined
   *          behavior will occur
   * @return Reference to parameter
   */
  virtual PluginParameter* operator[](const int i) const { return get(i); }
  /**
   * Lookup a parameter by index
   *
   * @param i Parameter index, must be less than the set's size or undefined
   *          behavior will occur
   * @return Reference to parameter
   */
  virtual PluginParameter* get(const int index) const { return parameterList.at(index); }
  /**
   * Lookup a parameter by name, for example: parameterSet["foo"]
   *
   * @param name The parameter's name
   * @return Reference to parameter, or NULL if not found
   */
  virtual PluginParameter* operator[](const ParameterString& name) const { return get(name); }
  /**
   * Lookup a parameter by name
   *
   * @param name The parameter's name
   * @return Reference to parameter, or NULL if not found
   */
  virtual PluginParameter* get(const ParameterString& name) const {
    ParameterMap::const_iterator iterator = parameterMap.find(PluginParameter::makeSafeName(name));
    return (iterator != parameterMap.end()) ? iterator->second : NULL;
  }

#if ENABLE_MULTITHREADED
  virtual void set(const ParameterString& name, const ParameterValue value,
    const bool realtime = false, PluginParameterObserver* sender = NULL) {
    PluginParameter* parameter = get(name);
    if(parameter != NULL) {
      set(parameter, value, realtime, sender);
    }
  }

  virtual void set(const int index, const ParameterValue value,
    bool realtime = false, PluginParameterObserver* sender = NULL) {
    return set(parameterList.at(index), value, realtime, sender);
  }

  virtual void set(PluginParameter* parameter, const ParameterValue value,
    bool realtime = false, PluginParameterObserver* sender = NULL) {
    scheduleEvent(new Event(parameter, value, realtime, sender));
  }

protected:
  virtual void scheduleEvent(Event* event) {
    if(event->isRealtime) {
      realtimeDispatcher.add(event);
    }
    else {
      asyncDispatcher.add(event);
    }
  }
#endif

private:
  typedef std::map<ParameterString, PluginParameter*> ParameterMap;
  typedef std::vector<PluginParameter*> ParameterList;

  ParameterMap parameterMap;
  ParameterList parameterList;
#if ENABLE_MULTITHREADED
  EventDispatcher asyncDispatcher;
  EventDispatcher realtimeDispatcher;
  EventDispatcherThread asyncDispatcherThread;
#endif
};
}

#endif
