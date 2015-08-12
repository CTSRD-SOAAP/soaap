#ifndef SOAAP_ADT_QUEUESET_H
#define SOAAP_ADT_QUEUESET_H

#include <set>
#include <list>

namespace soaap {

  template<typename T>
  class QueueSet {
    public:
      bool enqueue(T elem);
      T dequeue();
      bool empty();
      int size();
      void clear();

    protected:
      std::set<T> set;
      std::list<T> list;
  };

  template<typename T>
  bool QueueSet<T>::enqueue(T elem) {
    if (set.insert(elem).second) {
      list.push_back(elem);
      return true;
    }
    return false;
  }

  template<typename T>
  T QueueSet<T>::dequeue() {
    T elem = list.front();
    list.pop_front();
    set.erase(elem);
    return elem;
  }

  template<typename T>
  bool QueueSet<T>::empty() {
    return list.empty();
  }

  template<typename T>
  int QueueSet<T>::size() {
    return list.size();
  }

  template<typename T>
  void QueueSet<T>::clear() {
    list.clear();
    set.clear();
  }

}

#endif
