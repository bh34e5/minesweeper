#pragma once

#include "op.cc"

#include <assert.h>
#include <sys/types.h>

inline auto opDec(size_t v) -> Op<size_t> {
    return v == 0 ? Op<size_t>::empty() : v - 1;
}

inline auto opInc(size_t v, size_t max) -> Op<size_t> {
    return v == (max - 1) ? Op<size_t>::empty() : v + 1;
}

inline auto absDiff(size_t lhs, size_t rhs) -> size_t {
    return (rhs > lhs) ? rhs - lhs : lhs - rhs;
}

enum Dir {
    NORTHWEST,
    NORTH,
    NORTHEAST,
    EAST,
    SOUTHEAST,
    SOUTH,
    SOUTHWEST,
    WEST,
};

auto dirNorthward(Dir dir) -> bool {
    switch (dir) {
    case NORTHWEST:
    case NORTH:
    case NORTHEAST: {
        return true;
    }
    case EAST:
    case SOUTHEAST:
    case SOUTH:
    case SOUTHWEST:
    case WEST: {
        return false;
    }
    }
    assert(0 && "Unreachable");
}

auto dirSouthward(Dir dir) -> bool {
    switch (dir) {
    case SOUTHEAST:
    case SOUTH:
    case SOUTHWEST: {
        return true;
    }
    case NORTHWEST:
    case NORTH:
    case NORTHEAST:
    case EAST:
    case WEST: {
        return false;
    }
    }
    assert(0 && "Unreachable");
}

auto dirEastward(Dir dir) -> bool {
    switch (dir) {
    case NORTHEAST:
    case EAST:
    case SOUTHEAST: {
        return true;
    }
    case NORTHWEST:
    case NORTH:
    case SOUTH:
    case SOUTHWEST:
    case WEST: {
        return false;
    }
    }
    assert(0 && "Unreachable");
}

auto dirWestward(Dir dir) -> bool {
    switch (dir) {
    case NORTHWEST:
    case SOUTHWEST:
    case WEST: {
        return true;
    }
    case NORTH:
    case NORTHEAST:
    case EAST:
    case SOUTHEAST:
    case SOUTH: {
        return false;
    }
    }
    assert(0 && "Unreachable");
}

struct SLocation {
    ssize_t row;
    ssize_t col;

    auto eql(SLocation const &other) -> bool const {
        return this->row == other.row && this->col == other.col;
    }
};

struct Location {
    size_t row;
    size_t col;

    auto eql(Location const &other) -> bool const {
        return this->row == other.row && this->col == other.col;
    }
};

struct Dims {
    size_t width;
    size_t height;

    inline auto area() -> size_t { return this->width * this->height; }

    static auto fromCorners(Location ul, Location br) -> Dims {
        assert(ul.row <= br.row && "Invalid rect");
        assert(ul.col <= br.col && "Invalid rect");

        return Dims{br.col - ul.col, br.row - ul.row};
    }

    static auto fromCorners(SLocation ul, SLocation br) -> Dims {
        assert(ul.row <= br.row && "Invalid rect");
        assert(ul.col <= br.col && "Invalid rect");

        return Dims{static_cast<size_t>(br.col - ul.col),
                    static_cast<size_t>(br.row - ul.row)};
    }
};

struct SRect {
    SLocation ul;
    Dims dims;

    static auto fromCorners(SLocation ul, SLocation br) -> SRect {
        return SRect{ul, Dims::fromCorners(ul, br)};
    }

    auto ur() -> SLocation {
        ssize_t right_col = this->ul.col + this->dims.width;
        return SLocation{this->ul.row, right_col};
    }

    auto bl() -> SLocation {
        ssize_t bottom_row = this->ul.row + this->dims.height;
        return SLocation{bottom_row, this->ul.col};
    }

    auto br() -> SLocation {
        ssize_t right_col = this->ul.col + this->dims.width;
        ssize_t bottom_row = this->ul.row + this->dims.height;
        return SLocation{bottom_row, right_col};
    }
};

struct Rect {
    Location ul;
    Dims dims;

    static auto fromCorners(Location ul, Location br) -> Rect {
        return Rect{ul, Dims::fromCorners(ul, br)};
    }

    auto ur() -> Location {
        return Location{this->ul.row, this->ul.col + this->dims.width};
    }

    auto bl() -> Location {
        return Location{this->ul.row + this->dims.height, this->ul.col};
    }

    auto br() -> Location {
        return Location{this->ul.row + this->dims.height,
                        this->ul.col + this->dims.width};
    }
};

auto neighborOp(Location loc, Dims dims, Dir dir) -> Op<Location> {
    Op<size_t> r_ind = dirNorthward(dir)   ? opDec(loc.row)
                       : dirSouthward(dir) ? opInc(loc.row, dims.height)
                                           : loc.row;

    Op<size_t> c_ind = dirWestward(dir)   ? opDec(loc.col)
                       : dirEastward(dir) ? opInc(loc.col, dims.width)
                                          : loc.col;

    if (!r_ind.valid || !c_ind.valid) {
        return Op<Location>::empty();
    }

    size_t n_row = r_ind.get();
    size_t n_col = c_ind.get();
    return Location{n_row, n_col};
}

inline auto neighbor(Location loc, Dims dims, Dir dir) -> Location {
    return neighborOp(loc, dims, dir).get();
}

inline auto isNeighbor(Location l1, Location l2) -> bool {
    return (absDiff(l1.row, l2.row) <= 1) && (absDiff(l1.col, l2.col) <= 1);
}

struct NeighborIterator {
    Location loc;
    Dims dims;
    size_t dir;

    explicit NeighborIterator(Location loc, Dims dims)
        : loc(loc), dims(dims), dir(0) {}

    explicit NeighborIterator(size_t row, size_t col, Dims dims)
        : NeighborIterator(Location{row, col}, dims) {}

    auto next() -> Op<Location> {
        if (this->dir >= 8) {
            return Op<Location>::empty();
        }

        // safe case, we checked above
        auto dir = static_cast<Dir>(this->dir++);
        Op<Location> res_op = neighborOp(this->loc, this->dims, dir);
        if (!res_op.valid) {
            return this->next();
        }
        return res_op;
    }
};

auto neighborCount(Location loc, Dims dims) -> size_t {
    size_t count = 0;

    auto neighbor_op = Op<Location>::empty();
    auto neighbor_it = NeighborIterator{loc, dims};
    while ((neighbor_op = neighbor_it.next()).valid) {
        ++count;
    }

    return count;
}
