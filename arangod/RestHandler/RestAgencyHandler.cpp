
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
/// @author Kaveh Vahedipour
////////////////////////////////////////////////////////////////////////////////

#include "RestServer/ArangoServer.h"
#include "Rest/HttpRequest.h"
#include "Rest/Version.h"
#include "RestAgencyHandler.h"

#include "Agency/Agent.h"

#include <velocypack/Builder.h>
#include <velocypack/velocypack-aliases.h>

#include "Logger/Logger.h"

using namespace arangodb;

using namespace arangodb::basics;
using namespace arangodb::rest;
using namespace arangodb::consensus;

////////////////////////////////////////////////////////////////////////////////
/// @brief ArangoDB server
////////////////////////////////////////////////////////////////////////////////

extern ArangoServer* ArangoInstance;

RestAgencyHandler::RestAgencyHandler(HttpRequest* request, Agent* agent)
    : RestBaseHandler(request), _agent(agent) {
}

bool RestAgencyHandler::isDirect() const { return false; }

inline HttpHandler::status_t RestAgencyHandler::reportErrorEmptyRequest () {
  LOG(WARN) << "Empty request to public agency interface.";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::reportTooManySuffices () {
  LOG(WARN) << "Too many suffixes. Agency public interface takes one path.";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::reportUnknownMethod () {
  LOG(WARN) << "Too many suffixes. Agency public interface takes one path.";
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::redirect (id_t leader_id) {
  LOG(WARN) << "Redirecting request to " << leader_id;
  generateError(HttpResponse::NOT_FOUND,404);
  return HttpHandler::status_t(HANDLER_DONE);
}
#include <iostream>
inline HttpHandler::status_t RestAgencyHandler::handleWrite () {
  arangodb::velocypack::Options options; // TODO: User not wait. 
  if (_request->requestType() == HttpRequest::HTTP_REQUEST_POST) {
    query_t query;
    try {
      query = _request->toVelocyPack(&options);
    } catch (std::exception const& e) {
      LOG(FATAL) << e.what();
      generateError(HttpResponse::UNPROCESSABLE_ENTITY,422);
      return HttpHandler::status_t(HANDLER_DONE);
    }
    write_ret_t ret = _agent->write (query);
    size_t errors = 0;
    if (ret.accepted) {
      Builder body;
      body.add(VPackValue(VPackValueType::Object));
      _agent->waitFor (ret.indices.back()); // Wait for confirmation (last entry is enough)
      for (size_t i = 0; i < ret.indices.size(); ++i) {
        body.add(std::to_string(i), Value(ret.indices[i]));
        if (ret.indices[i] == 0) {
          errors++;
        }
      }
      body.close();
      if (errors > 0) { // epic fail
        generateResult(HttpResponse::PRECONDITION_FAILED,body.slice());
      } else {// full success
        generateResult(body.slice());
      }
    } else {
      //_response->setHeader("Location", _agent->config().end_points[ret.redirect]);
      generateError(HttpResponse::TEMPORARY_REDIRECT,307);
      return HttpHandler::status_t(HANDLER_DONE);
    }
  } else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED,405);
    return HttpHandler::status_t(HANDLER_DONE);
  }
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::handleRead () {
  arangodb::velocypack::Options options;
  if (_request->requestType() == HttpRequest::HTTP_REQUEST_POST) {
    query_t query;
    try {
      query = _request->toVelocyPack(&options);
    } catch (std::exception const& e) {
      LOG(FATAL) << e.what();
      generateError(HttpResponse::UNPROCESSABLE_ENTITY,422);
      return HttpHandler::status_t(HANDLER_DONE);
    }
    read_ret_t ret = _agent->read (query);
    if (ret.accepted) {
      generateResult(ret.result->slice());
    } else {
      generateError(HttpResponse::TEMPORARY_REDIRECT,307);
      return HttpHandler::status_t(HANDLER_DONE);
    }
  } else {
    generateError(HttpResponse::METHOD_NOT_ALLOWED,405);
    return HttpHandler::status_t(HANDLER_DONE);
  }
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyHandler::handleTest() {
  Builder body;
  body.add(VPackValue(VPackValueType::Object));
  body.add("id", Value(_agent->id()));
  body.add("term", Value(_agent->term()));
  body.add("leaderId", Value(_agent->leaderID()));
  body.add("configuration", _agent->config().toBuilder()->slice());
  body.close();
  generateResult(body.slice());
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyHandler::handleState() {
  Builder body;
  body.add(VPackValue(VPackValueType::Array));
  for (auto const& i: _agent->state().slices())
    body.add(i);
  body.close();
  generateResult(body.slice());
  return HttpHandler::status_t(HANDLER_DONE);
}

inline HttpHandler::status_t RestAgencyHandler::reportMethodNotAllowed () {
  generateError(HttpResponse::METHOD_NOT_ALLOWED,405);
  return HttpHandler::status_t(HANDLER_DONE);
}

HttpHandler::status_t RestAgencyHandler::execute() {
  try {
    if (_request->suffix().size() == 0) {         // Empty request
      return reportErrorEmptyRequest();
    } else if (_request->suffix().size() > 1) {   // path size >= 2
      return reportTooManySuffices();
    } else {
    	if (_request->suffix()[0] == "write") {
        return handleWrite();
      } else if (_request->suffix()[0] == "read") {
        return handleRead();
      } else if (_request->suffix()[0] == "config") {
        if (_request->requestType() != HttpRequest::HTTP_REQUEST_GET) {
          return reportMethodNotAllowed();
        }
        return handleTest();
    	} else if (_request->suffix()[0] == "state") {
        if (_request->requestType() != HttpRequest::HTTP_REQUEST_GET) {
          return reportMethodNotAllowed();
        }
        return handleState();
    	} else {
        return reportUnknownMethod();
    	}
    }
  } catch (...) {
    // Ignore this error
  }
  return HttpHandler::status_t(HANDLER_DONE);
}