#pragma once

template <typename T> struct LinkedList {
    T val;
    LinkedList *next;
    LinkedList *prev;

    static auto initSentinel(LinkedList *ll) -> void {
        ll->next = ll;
        ll->prev = ll;
    }

    static auto isEmpty(LinkedList *ll) -> bool { return ll->next == ll; }

    auto enqueue(LinkedList *ll) -> void {
        ll->prev = this->prev;
        ll->next = this;

        ll->prev->next = ll;
        ll->next->prev = ll;
    }

    auto dequeue() -> LinkedList * {
        if (this->next == this) {
            return nullptr;
        }

        LinkedList *ll = this->next;
        ll->prev->next = ll->next;
        ll->next->prev = ll->prev;

        ll->prev = nullptr;
        ll->next = nullptr;

        return ll;
    }
};
