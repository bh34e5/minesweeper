#pragma once

#include "../arena.cc"
#include "../dirutils.cc"
#include "../linkedlist.cc"

#include <assert.h>
#include <stddef.h>

struct BoxItem {
    Dims dims;
    size_t flex_factor;
};

struct HBox {
    struct LocIterator {
        LinkedList<BoxItem> *cur;
        LinkedList<BoxItem> *end;
        SLocation cur_loc;
        size_t gap;
        size_t height;
        size_t total_flex;
        size_t remaining_width;

        auto getNext() -> SRect {
            assert(this->hasNext() && "Invalid iterator");

            BoxItem item = this->cur->val;
            Dims cur_dims = item.dims;
            if (this->total_flex) {
                cur_dims.width += (size_t)((double)item.flex_factor *
                                           (double)this->remaining_width /
                                           (double)this->total_flex);
            }

            SLocation cur_loc = this->cur_loc;

            this->cur = this->cur->next;
            this->cur_loc.col += cur_dims.width + this->gap;

            return SRect{cur_loc, Dims{cur_dims.width, this->height}};
        }

        auto hasNext() -> bool { return cur != end; }
    };

    size_t gap;
    size_t fill_width;

    Dims total_dims;
    size_t total_flex;
    LinkedList<BoxItem> items_sentinel;

    auto pushItem(Arena *arena, Dims dims, size_t flex_factor = 0) -> void {
        LinkedList<BoxItem> *element =
            arena->pushT<LinkedList<BoxItem>>({{dims, flex_factor}});

        if (!LinkedList<BoxItem>::isEmpty(&this->items_sentinel)) {
            this->total_dims.width += this->gap;
        }
        this->total_dims.width += dims.width;

        if (this->total_dims.height < dims.height) {
            this->total_dims.height = dims.height;
        }

        this->items_sentinel.enqueue(element);
        this->total_flex += flex_factor;
    }

    auto itemsIterator(SLocation base, size_t min_height = 0) -> LocIterator {
        size_t remaining_width = 0;
        if (this->total_dims.width < this->fill_width) {
            remaining_width = this->fill_width - this->total_dims.width;
        }

        size_t height = this->total_dims.height;
        if (height < min_height) {
            height = min_height;
        }

        return LocIterator{this->items_sentinel.next,
                           &this->items_sentinel,
                           base,
                           this->gap,
                           height,
                           this->total_flex,
                           remaining_width};
    }

    auto getDims() -> Dims {
        Dims dims = this->total_dims;
        if (dims.width < this->fill_width) {
            dims.width = this->fill_width;
        }
        return dims;
    }
};

auto initHBox(HBox *hbox, size_t gap = 0, size_t fill_width = 0) -> void {
    *hbox = HBox{gap, fill_width};
    LinkedList<BoxItem>::initSentinel(&hbox->items_sentinel);
}

struct VBox {
    struct LocIterator {
        LinkedList<BoxItem> *cur;
        LinkedList<BoxItem> *end;
        SLocation cur_loc;
        size_t gap;
        size_t width;
        size_t total_flex;
        size_t remaining_height;

        auto getNext() -> SRect {
            assert(this->hasNext() && "Invalid iterator");

            BoxItem item = this->cur->val;
            Dims cur_dims = item.dims;
            if (this->total_flex) {
                cur_dims.height += (size_t)((double)item.flex_factor *
                                            (double)this->remaining_height /
                                            (double)this->total_flex);
            }

            SLocation cur_loc = this->cur_loc;

            this->cur = this->cur->next;
            this->cur_loc.row += cur_dims.height + this->gap;

            return SRect{cur_loc, Dims{this->width, cur_dims.height}};
        }

        auto hasNext() -> bool { return cur != end; }
    };

    size_t gap;
    size_t fill_height;

    Dims total_dims;
    size_t total_flex;
    LinkedList<BoxItem> items_sentinel;

    auto pushItem(Arena *arena, Dims dims, size_t flex_factor = 0) -> void {
        LinkedList<BoxItem> *element =
            arena->pushT<LinkedList<BoxItem>>({{dims, flex_factor}});

        if (!LinkedList<BoxItem>::isEmpty(&this->items_sentinel)) {
            this->total_dims.height += this->gap;
        }
        this->total_dims.height += dims.height;

        if (this->total_dims.width < dims.width) {
            this->total_dims.width = dims.width;
        }

        this->items_sentinel.enqueue(element);
        this->total_flex += flex_factor;
    }

    auto itemsIterator(SLocation base, size_t min_width = 0) -> LocIterator {
        size_t remaining_height = 0;
        if (this->total_dims.height < this->fill_height) {
            remaining_height = this->fill_height - this->total_dims.height;
        }

        size_t width = this->total_dims.width;
        if (width < min_width) {
            width = min_width;
        }

        return LocIterator{this->items_sentinel.next,
                           &this->items_sentinel,
                           base,
                           this->gap,
                           width,
                           this->total_flex,
                           remaining_height};
    }

    auto getDims() -> Dims {
        Dims dims = this->total_dims;
        if (dims.height < this->fill_height) {
            dims.height = this->fill_height;
        }
        return dims;
    }
};

auto initVBox(VBox *vbox, size_t gap = 0, size_t fill_height = 0) -> void {
    *vbox = VBox{gap, fill_height};
    LinkedList<BoxItem>::initSentinel(&vbox->items_sentinel);
}
