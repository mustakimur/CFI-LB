/*
 * Project: Adaptive Control Flow Integrity with Look Back and Multi-scope CFG
 * Generation Author: Mustakimur Rahman Khandaker (mrk15e@my.fsu.edu) Florida
 * State University Supervised: Dr. Zhi Wang
 */
struct node {
  unsigned long long data;
  struct node *next;
};

// stack implementation using linked list
class stack {
  struct node *top;

 public:
  stack() { top = NULL; }
  void push(unsigned long long);
  unsigned long long pop();
  unsigned long long head1();
  unsigned long long head2();
  unsigned long long head3();
  bool isEmpty();
  bool isStatusValid();
};

// push a function entry into the stack
void stack::push(unsigned long long value) {
  struct node *ptr;
  ptr = new node;
  ptr->data = value;
  ptr->next = NULL;
  if (top != NULL) ptr->next = top;
  top = ptr;
}

// pop the recent function entry from the stack
unsigned long long stack::pop() {
  unsigned long long value = top->data;
  top = top->next;
  return value;
}

// top of the stack
unsigned long long stack::head1() {
  struct node *ptr1 = top;
  return ptr1->data;
}

unsigned long long stack::head2() {
  struct node *ptr2 = top->next;
  return ptr2->data;
}

unsigned long long stack::head3() {
  struct node *ptr3 = top->next->next;
  return ptr3->data;
}

bool stack::isEmpty() {
  if (top == NULL) {
    return true;
  }
  return false;
}

bool stack::isStatusValid() {
  if (top != NULL && top->next != NULL && top->next->next != NULL) {
    return true;
  }
  return false;
}
