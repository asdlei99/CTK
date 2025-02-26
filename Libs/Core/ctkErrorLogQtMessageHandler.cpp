/*=========================================================================

  Library:   CTK

  Copyright (c) Kitware Inc.

  Licensed under the Apache License, Version 2.0 (the "License");
  you may not use this file except in compliance with the License.
  You may obtain a copy of the License at

      http://www.apache.org/licenses/LICENSE-2.0.txt

  Unless required by applicable law or agreed to in writing, software
  distributed under the License is distributed on an "AS IS" BASIS,
  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
  See the License for the specific language governing permissions and
  limitations under the License.

=========================================================================*/

// Qt includes
#include <QAtomicInt>
#include <QCoreApplication>
#include <QThread>
#include <QVariant>

// CTK includes
#include "ctkErrorLogContext.h"
#include "ctkErrorLogQtMessageHandler.h"
#include <ctkUtils.h>

// STD includes
#include <iostream>

// Handling log messages may generate log messages, which would cause infinite loop.
// We stop handling log messages if the maximum recursion depth is reached.
static QAtomicInt ctkErrorLogQtMessageHandler_CurrentRecursionDepth;

//------------------------------------------------------------------------------
QString ctkErrorLogQtMessageHandler::HandlerName = QLatin1String("Qt");

//------------------------------------------------------------------------------
//Q_DECLARE_METATYPE(QPointer<ctkErrorLogQtMessageHandler>)
Q_DECLARE_METATYPE(ctkErrorLogQtMessageHandler*)

// --------------------------------------------------------------------------
ctkErrorLogQtMessageHandler::ctkErrorLogQtMessageHandler(QObject* parent)
  : Superclass(parent)
{
  this->SavedQtMessageHandler = 0;

  QCoreApplication * coreApp = QCoreApplication::instance();

  // Keep track of all instantiated ctkErrorLogModel
  QList<QVariant> handlers = coreApp->property("ctkErrorLogQtMessageHandlers").toList();
  handlers << QVariant::fromValue(this);
  //handlers << QVariant::fromValue(QPointer<ctkErrorLogQtMessageHandler>(this));
  coreApp->setProperty("ctkErrorLogQtMessageHandlers", handlers);
}

namespace
{
//------------------------------------------------------------------------------
void ctkErrorLogModelQtMessageOutput(QtMsgType type, const QMessageLogContext& context,
                                     const QString& msg)
{
  ctkErrorLogQtMessageHandler_CurrentRecursionDepth.ref();
  // Allow a couple of recursion levels to get a hint about where and why recursion occurs,
  // so we stop processing the message if recursion depth is over 10.
#if QT_VERSION >= QT_VERSION_CHECK(5,14,0)
  if (ctkErrorLogQtMessageHandler_CurrentRecursionDepth.loadRelaxed() > 10)
#else
  if (ctkErrorLogQtMessageHandler_CurrentRecursionDepth.load() > 10)
#endif
    {
    ctkErrorLogQtMessageHandler_CurrentRecursionDepth.deref();
    return;
    }

  //TODO: use context in the log message
  Q_UNUSED(context)
  // Warning: To avoid infinite loop, do not use Q_ASSERT in this function.
  if (msg.isEmpty())
    {
    ctkErrorLogQtMessageHandler_CurrentRecursionDepth.deref();
    return;
    }
  ctkErrorLogLevel::LogLevel level = ctkErrorLogLevel::Unknown;
  if (type == QtDebugMsg)
    {
    level = ctkErrorLogLevel::Debug;
    }
#if QT_VERSION >= QT_VERSION_CHECK(5,5,0)
  else if (type == QtInfoMsg)
    {
    level = ctkErrorLogLevel::Info;
    }
#endif
  else if (type == QtWarningMsg)
    {
    level = ctkErrorLogLevel::Warning;
    }
  else if (type == QtCriticalMsg)
    {
    level = ctkErrorLogLevel::Critical;
    }
  else if (type == QtFatalMsg)
    {
    level = ctkErrorLogLevel::Fatal;
    }

  QCoreApplication * coreApp = QCoreApplication::instance();
  QList<QVariant> handlers = coreApp->property("ctkErrorLogQtMessageHandlers").toList();
  foreach(QVariant v, handlers)
    {
    ctkErrorLogQtMessageHandler* handler = v.value<ctkErrorLogQtMessageHandler*>();
    Q_ASSERT(handler);
//    //QPointer<ctkErrorLogQtMessageHandler> handler = v.value<QPointer<ctkErrorLogQtMessageHandler> >();
//    //if(handler.isNull())
//    //  {
//    //  continue;
//    //  }
    handler->handleMessage(
          ctk::qtHandleToString(QThread::currentThreadId()),
          level, handler->handlerPrettyName(), ctkErrorLogContext(msg), msg);
    }
  ctkErrorLogQtMessageHandler_CurrentRecursionDepth.deref();
}
}

// --------------------------------------------------------------------------
QString ctkErrorLogQtMessageHandler::handlerName()const
{
  return ctkErrorLogQtMessageHandler::HandlerName;
}

// --------------------------------------------------------------------------
void ctkErrorLogQtMessageHandler::setEnabledInternal(bool value)
{
  if (value)
    {
    this->SavedQtMessageHandler = qInstallMessageHandler(ctkErrorLogModelQtMessageOutput);
    }
  else
    {
    qInstallMessageHandler(this->SavedQtMessageHandler);
    }
}
