#ifndef _HASH_MAP_H
#define _HASH_MAP_H

#include <stddef.h>

// #define LOG_COLLISION

#ifndef LOG_TRACE
// clang-format off
#define LOG_TRACE(m) do { } while (false)
// clang-format on
#endif

template <typename K, typename V> class HashNode
{
private:
  // key-value pair
  K _key;
  V _value;
  // next bucket with the same key
  HashNode* _next;
  // disallow copy and assignment
  HashNode(const HashNode&);
  HashNode& operator=(const HashNode&);

public:
  HashNode(const K& key, V& value) : _key(key), _value(value), _next(NULL) {}

  K getKey() const { return _key; }

  V getValue() const { return _value; }

  void setValue(const V& value) { _value = value; }

  HashNode* getNext() const { return _next; }

  void setNext(HashNode* next) { _next = next; }
};

template <typename K, typename V, size_t tableSize> class HashMap
{
private:
  HashMap(const HashMap& other);
  const HashMap& operator=(const HashMap& other);
  // hash table
  HashNode<K, V>* table[tableSize];

  uint16_t (*hashFunc)(K& key);

public:
  HashMap(uint16_t (*hashFunc)(K& key)) : table(), hashFunc(hashFunc) {}

  ~HashMap()
  {
    // destroy all buckets one by one
    for (size_t i = 0; i < tableSize; ++i)
    {
      HashNode<K, V>* entry = table[i];

      while (entry != NULL)
      {
        HashNode<K, V>* prev = entry;
        entry = entry->getNext();
        delete prev;
      }

      table[i] = NULL;
    }
  }

  uint16_t calculateHashKey(K key) { return hashFunc(key) % tableSize; }

  V get(const K& key)
  {
    uint16_t hashKey = calculateHashKey(key);
    HashNode<K, V>* entry = table[hashKey];

    while (entry != NULL)
    {
      if (entry->getKey() == key)
      {
        return entry->getValue();
      }

      entry = entry->getNext();
    }

    return nullptr;
  }

  void put(const K& key, V& value)
  {
    uint16_t hashKey = calculateHashKey(key);
    HashNode<K, V>* prev = NULL;
    HashNode<K, V>* entry = table[hashKey];

    while (entry != NULL && entry->getKey() != key)
    {
#ifdef LOG_COLLISION
      LOG_TRACE(F("Key collision: ") << hashKey);
#endif
      prev = entry;
      entry = entry->getNext();
    }

    if (entry == NULL)
    {
      entry = new HashNode<K, V>(key, value);

      if (prev == NULL)
      {
        // insert as first bucket
        table[hashKey] = entry;
      }
      else
      {
        prev->setNext(entry);
      }
    }
    else
    {
      // just update the value
      entry->setValue(value);
    }
  }

  void remove(const K& key)
  {
    uint16_t hashKey = calculateHashKey(key);
    HashNode<K, V>* prev = NULL;
    HashNode<K, V>* entry = table[hashKey];

    while (entry != NULL && entry->getKey() != key)
    {
      prev = entry;
      entry = entry->getNext();
    }

    if (entry == NULL)
    {
      // key not found
      return;
    }
    else
    {
      if (prev == NULL)
      {
        // remove first bucket of the list
        table[hashKey] = entry->getNext();
      }
      else
      {
        prev->setNext(entry->getNext());
      }

      delete entry;
    }
  }
};

#endif // _HASH_MAP_H
