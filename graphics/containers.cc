#pragma once

#include "../arena.cc"
#include "../dirutils.cc"
#include "../linkedlist.cc"

#include <assert.h>
#include <stddef.h>

struct HBox {
    struct LocIterator {
        LinkedList<Dims> *cur;
        LinkedList<Dims> *end;
        SLocation cur_loc;
        size_t gap;
        size_t height;

        auto getNext() -> SRect {
            assert(this->hasNext() && "Invalid iterator");

            Dims cur_dims = this->cur->val;
            SLocation cur_loc = this->cur_loc;

            this->cur = this->cur->next;
            this->cur_loc.col += cur_dims.width + this->gap;

            return SRect{cur_loc, Dims{cur_dims.width, this->height}};
        }

        auto hasNext() -> bool { return cur != end; }
    };

    size_t gap;
    Dims total_dims;
    LinkedList<Dims> items_sentinel;

    auto pushItem(Arena *arena, Dims dims) -> void {
        LinkedList<Dims> *element = arena->pushT<LinkedList<Dims>>({dims});

        if (!LinkedList<Dims>::isEmpty(&this->items_sentinel)) {
            this->total_dims.width += this->gap;
        }
        this->total_dims.width += dims.width;

        if (this->total_dims.height < dims.height) {
            this->total_dims.height = dims.height;
        }

        this->items_sentinel.enqueue(element);
    }

    auto itemsIterator(SLocation base) -> LocIterator {
        return LocIterator{this->items_sentinel.next, &this->items_sentinel,
                           base, this->gap, this->total_dims.height};
    }
};

auto initHBox(HBox *hbox, size_t gap = 0) -> void {
    *hbox = HBox{gap};
    LinkedList<Dims>::initSentinel(&hbox->items_sentinel);
}

struct VBox {
    struct LocIterator {
        LinkedList<Dims> *cur;
        LinkedList<Dims> *end;
        SLocation cur_loc;
        size_t gap;
        size_t width;

        auto getNext() -> SRect {
            assert(this->hasNext() && "Invalid iterator");

            Dims cur_dims = this->cur->val;
            SLocation cur_loc = this->cur_loc;

            this->cur = this->cur->next;
            this->cur_loc.row += cur_dims.height + this->gap;

            return SRect{cur_loc, Dims{this->width, cur_dims.height}};
        }

        auto hasNext() -> bool { return cur != end; }
    };

    size_t gap;
    Dims total_dims;
    LinkedList<Dims> items_sentinel;

    auto pushItem(Arena *arena, Dims dims) -> void {
        LinkedList<Dims> *element = arena->pushT<LinkedList<Dims>>({dims});

        if (!LinkedList<Dims>::isEmpty(&this->items_sentinel)) {
            this->total_dims.height += this->gap;
        }
        this->total_dims.height += dims.height;

        if (this->total_dims.width < dims.width) {
            this->total_dims.width = dims.width;
        }

        this->items_sentinel.enqueue(element);
    }

    auto itemsIterator(SLocation base) -> LocIterator {
        return LocIterator{this->items_sentinel.next, &this->items_sentinel,
                           base, this->gap, this->total_dims.width};
    }
};

auto initVBox(VBox *vbox, size_t gap = 0) -> void {
    *vbox = VBox{gap};
    LinkedList<Dims>::initSentinel(&vbox->items_sentinel);
}
