# Lock Implementation with Atomic Load & Store

---

## Implementation

```python
def lock(lock) {
  while (True):
    while (atomic_load(lock.value) != 0):
      thread_yield()
  
    atomic_store(lock.value, thread_id)
    if (atomic_load(lock.value) == thread_id):
      break
}
```

```python
def unlock(lock) {
  atomic_store(lock, 0);
}
```
