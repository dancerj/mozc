// Copyright 2010-2020, Google Inc.
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without
// modification, are permitted provided that the following conditions are
// met:
//
//     * Redistributions of source code must retain the above copyright
// notice, this list of conditions and the following disclaimer.
//     * Redistributions in binary form must reproduce the above
// copyright notice, this list of conditions and the following disclaimer
// in the documentation and/or other materials provided with the
// distribution.
//     * Neither the name of Google Inc. nor the names of its
// contributors may be used to endorse or promote products derived from
// this software without specific prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
// "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
// LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
// A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
// OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
// LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
// DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
// THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
// (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
// OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#ifndef MOZC_SESSION_GENERIC_STORAGE_MANAGER_H_
#define MOZC_SESSION_GENERIC_STORAGE_MANAGER_H_

#include <memory>

#include "base/port.h"
#include "protocol/commands.pb.h"

namespace mozc {

class GenericStorageInterface;

// For unit test.
class GenericLruStorageProxy;

namespace storage {
class LRUStorage;
}  // namespace storage

// Override and set the subclass's instance to
// GenericStorageManager for unit test.
class GenericStorageManagerInterface {
 public:
  virtual ~GenericStorageManagerInterface() = default;

  virtual GenericStorageInterface *GetStorage(
      commands::GenericStorageEntry::StorageType storage_type) = 0;

  // Synchronizes all the managed storages.  Returns true iff all the storages
  // are synchronized successfully (Note: even if one failed, it's guaranteed
  // that Sync() to all the storages are called.).
  virtual bool SyncAll() = 0;
};

// Manages generic storages.
class GenericStorageManagerFactory {
 public:
  // Returns corresponding storage's instance.
  // If no instance is available, NULL is returned.
  static GenericStorageInterface *GetStorage(
      commands::GenericStorageEntry::StorageType storage_type);

  // Synchronizes all the storages managed by this factory.  Returns true iff
  // all the storages are synchronized successfully (Note: even if one failed,
  // it's guaranteed that Sync() to all the storages are called.).
  static bool SyncAll();

  // For unit test.
  static void SetGenericStorageManager(GenericStorageManagerInterface *manager);

  GenericStorageManagerFactory() = delete;
  GenericStorageManagerFactory(const GenericStorageManagerFactory &) = delete;
  GenericStorageManagerFactory &operator=(
      const GenericStorageManagerFactory &) = delete;
};

// Generic interface for storages.
// This class defines only the interfaces.
// Detailed behaviors depend on the subclass's
// backend.
class GenericStorageInterface {
 public:
  virtual ~GenericStorageInterface() = default;

  // Inserts new entry.
  // If something goes wrong, returns false.
  // value should be terminated by '\0'.
  virtual bool Insert(const std::string &key, const char *value) = 0;
  // Looks up the value.
  // If something goes wrong, returns NULL.
  virtual const char *Lookup(const std::string &key) = 0;
  // Lists all the values.
  // If something goes wrong, returns false.
  virtual bool GetAllValues(std::vector<std::string> *values) = 0;
  // Clears all the entries.
  virtual bool Clear() = 0;
  // Writes the data to file(s).
  virtual bool Sync() = 0;
};

// Storage class of which backend is LRUStorage.
class GenericLruStorage : public GenericStorageInterface {
 public:
  GenericLruStorage(const char *file_name, size_t value_size, size_t size,
                    uint32 seed);

  GenericLruStorage(const GenericLruStorage &) = delete;
  GenericLruStorage &operator=(const GenericLruStorage &) = delete;

  ~GenericLruStorage() override;

  // If the storage has |key|, this method overwrites
  // the old value.
  // If the entiry's size is over GetSize(),
  // the oldest value is disposed.
  bool Insert(const std::string &key, const char *value) override;

  const char *Lookup(const std::string &key) override;

  // The order is new to old.
  bool GetAllValues(std::vector<std::string> *values) override;

  bool Clear() override;
  bool Sync() override;

 protected:
  // Opens the storage if not opened yet.
  // If something goes wrong, returns false.
  bool EnsureStorage();

 private:
  friend class GenericLruStorageProxy;
  std::unique_ptr<mozc::storage::LRUStorage> lru_storage_;
  const std::string file_name_;
  const size_t value_size_;
  const size_t size_;
  const uint32 seed_;
  // Temporary buffer to insert a value into this storage.
  std::unique_ptr<char[]> value_buffer_;
};

}  // namespace mozc

#endif  // MOZC_SESSION_GENERIC_STORAGE_MANAGER_H_
