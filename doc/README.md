# Lock Implementation with Atomic Load & Store

---

## Implementation

```python
def lock(lock) {
  while (atomic_load(lock.value) != 0) {
    atomic_store(lock.value, thread_id);
    if (atomic_load(lock.value) == thread_id) {
      break;
    }
    thread_block(thread_id);
  }
}
```

```python
def unlock(lock) {
  atomic_store(lock, 0);
}
```
