/* This file is part of Clementine.
   Copyright 2011, David Sansome <me@davidsansome.com>

   Clementine is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 3 of the License, or
   (at your option) any later version.

   Clementine is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Clementine.  If not, see <http://www.gnu.org/licenses/>.
*/

#include "closure.h"

#include "core/logging.h"

Closure::Closure(QObject* sender,
                 const char* signal,
                 QObject* receiver,
                 const char* slot,
                 const ClosureArgumentWrapper* val0,
                 const ClosureArgumentWrapper* val1,
                 const ClosureArgumentWrapper* val2)
    : QObject(receiver),
      callback_(NULL),
      val0_(val0),
      val1_(val1),
      val2_(val2) {
  const QMetaObject* meta_receiver = receiver->metaObject();

  QByteArray normalised_slot = QMetaObject::normalizedSignature(slot + 1);
  const int index = meta_receiver->indexOfSlot(normalised_slot.constData());
  Q_ASSERT(index != -1);
  slot_ = meta_receiver->method(index);

  Connect(sender, signal);
}

Closure::Closure(QObject* sender,
                 const char* signal,
                 std::tr1::function<void()> callback)
    : callback_(callback) {
  Connect(sender, signal);
}

void Closure::Connect(QObject* sender, const char* signal) {
  bool success = connect(sender, signal, SLOT(Invoked()));
  Q_ASSERT(success);
  success = connect(sender, SIGNAL(destroyed()), SLOT(Cleanup()));
  Q_ASSERT(success);
  Q_UNUSED(success);
}

void Closure::Invoked() {
  if (callback_) {
    callback_();
  } else {
    slot_.invoke(
        parent(),
        val0_ ? val0_->arg() : QGenericArgument(),
        val1_ ? val1_->arg() : QGenericArgument(),
        val2_ ? val2_->arg() : QGenericArgument());
  }
  deleteLater();
}

void Closure::Cleanup() {
  disconnect();
  deleteLater();
}

Closure* NewClosure(
    QObject* sender, const char* signal,
    QObject* receiver, const char* slot) {
  return new Closure(sender, signal, receiver, slot);
}
