/* Copyright 2015, 2016 Gerald Baier
 *
 * This file is part of despeckCL.
 *
 * despeckCL is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * despeckCL is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with despeckCL. If not, see <http://www.gnu.org/licenses/>.
 */

#include "tile_iterator.h"

tile_iterator::tile_iterator(const int max_height,
                             const int max_width,
                             const int tile_size,
                             const int overlap_border,
                             const int overlap_tile) : max_height(max_height),
                                                       max_width(max_width),
                                                       tile_height(tile_size),
                                                       tile_width(tile_size),
                                                       overlap_border(overlap_border),
                                                       overlap_tile(overlap_tile),
                                                       h_low(-overlap_border),
                                                       w_low(-overlap_border) {}

tile_iterator::tile_iterator(const int max_height,
                             const int max_width,
                             const int tile_height,
                             const int tile_width,
                             const int overlap_border,
                             const int overlap_tile) : max_height(max_height),
                                                       max_width(max_width),
                                                       tile_height(tile_height),
                                                       tile_width(tile_width),
                                                       overlap_border(overlap_border),
                                                       overlap_tile(overlap_tile),
                                                       h_low(-overlap_border),
                                                       w_low(-overlap_border) {}

void tile_iterator::operator++()
{
    w_low += tile_width - 2*overlap_tile;
    if (w_low >= max_width) {
        w_low = -overlap_border;
        h_low += tile_height - 2*overlap_tile;
    }
}

bool tile_iterator::operator!=(const tile_iterator&) const
{
    return h_low < max_height;
}

tile<2> tile_iterator::operator*() const
{
    return tile<2>{slice{h_low, h_low+tile_height}, slice{w_low, w_low + tile_width}};
}

const tile_iterator& tile_iterator::begin() const
{
    return *this;
}

const tile_iterator& tile_iterator::end() const
{
    return *this;
}
