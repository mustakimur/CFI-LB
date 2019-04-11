/*
 * CFI-LB: Adaptive Call-site Sensitive Control Flow Integrity
 * Authors: Mustakimur Khandaker (Florida State University)
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
};

// push a call-site into the call stack
void stack::push(unsigned long long value) {
  struct node *ptr;
  ptr = new node;
  ptr->data = value;
  ptr->next = NULL;
  if (top != NULL) ptr->next = top;
  top = ptr;
}

// pop the recent call-site from the call stack
unsigned long long stack::pop() {
  unsigned long long value = top->data;
  struct node *ptr;
  ptr = top->next;
  delete top;
  top = ptr;
  return value;
}

// call-site level 1
unsigned long long stack::head1() {
  struct node *ptr1 = top;
  return ptr1->data;
}

// call-site level 2
unsigned long long stack::head2() {
  struct node *ptr2 = top->next;
  return ptr2->data;
}

// call-site level 3
unsigned long long stack::head3() {
  struct node *ptr3 = top->next->next;
  return ptr3->data;
}

// check if it has a minimum of three call-site
bool stack::isEmpty() {
  if (top == NULL || top->next == NULL || top->next->next == NULL) {
    return true;
  }
  return false;
}
