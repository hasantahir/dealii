// ---------------------------------------------------------------------
//
// Copyright (C) 2003 - 2019 by the deal.II authors
//
// This file is part of the deal.II library.
//
// The deal.II library is free software; you can use it, redistribute
// it, and/or modify it under the terms of the GNU Lesser General
// Public License as published by the Free Software Foundation; either
// version 2.1 of the License, or (at your option) any later version.
// The full text of the license can be found in the file LICENSE.md at
// the top level directory of deal.II.
//
// ---------------------------------------------------------------------

#include <deal.II/base/memory_consumption.h>

#include <deal.II/hp/dof_level.h>
#include <deal.II/hp/fe_collection.h>

#include <iostream>

DEAL_II_NAMESPACE_OPEN

namespace internal
{
  namespace hp
  {
    const DoFLevel::active_fe_index_type DoFLevel::invalid_active_fe_index =
      static_cast<DoFLevel::active_fe_index_type>(-1);



    template <int dim, int spacedim>
    void
    DoFLevel::compress_data(
      const dealii::hp::FECollection<dim, spacedim> &fe_collection)
    {
      (void)fe_collection;

      if (dof_offsets.size() == 0 || dof_indices.size() == 0)
        return;

      // in a first run through, count how many new slots we need in the
      // dof_indices array after compression. note that the 'cell'
      // counter is incremented inside the loop
      unsigned int new_size = 0;
      for (unsigned int cell = 0; cell < dof_offsets.size();)
        // see if this cell is active on the current level
        if (dof_offsets[cell] != static_cast<offset_type>(-1))
          {
            // find the next cell active on this level
            unsigned int next_cell = cell + 1;
            while ((next_cell < dof_offsets.size()) &&
                   (dof_offsets[next_cell] == static_cast<offset_type>(-1)))
              ++next_cell;

            const unsigned int next_offset =
              (next_cell < dof_offsets.size() ? dof_offsets[next_cell] :
                                                dof_indices.size());

            Assert(next_offset - dof_offsets[cell] ==
                     fe_collection[active_fe_indices[cell]]
                       .template n_dofs_per_object<dim>(),
                   ExcInternalError());

            // see if the range of dofs for this cell can be compressed and if
            // so how many slots we have to store for them
            if (next_offset > dof_offsets[cell])
              {
                bool compressible = true;
                for (unsigned int j = dof_offsets[cell] + 1; j < next_offset;
                     ++j)
                  if (dof_indices[j] != dof_indices[j - 1] + 1)
                    {
                      compressible = false;
                      break;
                    }
                if (compressible == true)
                  new_size += 1;
                else
                  new_size += (next_offset - dof_offsets[cell]);
              }

            // then move on to the next cell
            cell = next_cell;
          }
        else
          ++cell;

      // now allocate the new array and copy into it whatever we need
      std::vector<types::global_dof_index> new_dof_indices;
      new_dof_indices.reserve(new_size);
      std::vector<offset_type> new_dof_offsets(dof_offsets.size(),
                                               static_cast<offset_type>(-1));
      for (unsigned int cell = 0; cell < dof_offsets.size();)
        // see if this cell is active on the current level
        if (dof_offsets[cell] != static_cast<offset_type>(-1))
          {
            // find the next cell active on this level
            unsigned int next_cell = cell + 1;
            while ((next_cell < dof_offsets.size()) &&
                   (dof_offsets[next_cell] == static_cast<offset_type>(-1)))
              ++next_cell;

            const unsigned int next_offset =
              (next_cell < dof_offsets.size() ? dof_offsets[next_cell] :
                                                dof_indices.size());

            Assert(next_offset - dof_offsets[cell] ==
                     fe_collection[active_fe_indices[cell]]
                       .template n_dofs_per_object<dim>(),
                   ExcInternalError());

            new_dof_offsets[cell] = new_dof_indices.size();

            // see if the range of dofs for this cell can be compressed and if
            // so how many slots we have to store for them
            if (next_offset > dof_offsets[cell])
              {
                bool compressible = true;
                for (unsigned int j = dof_offsets[cell] + 1; j < next_offset;
                     ++j)
                  if (dof_indices[j] != dof_indices[j - 1] + 1)
                    {
                      compressible = false;
                      break;
                    }

                // if this cell is compressible, then copy the first index and
                // mark this in the dof_offsets array
                if (compressible == true)
                  {
                    new_dof_indices.push_back(dof_indices[dof_offsets[cell]]);

                    // make sure that the current active_fe_index indicates
                    // that this entry hasn't been compressed yet
                    Assert(is_compressed_entry(active_fe_indices[cell]) ==
                             false,
                           ExcInternalError());

                    // then mark the compression
                    active_fe_indices[cell] =
                      get_toggled_compression_state(active_fe_indices[cell]);
                  }
                else
                  for (unsigned int i = dof_offsets[cell]; i < next_offset; ++i)
                    new_dof_indices.push_back(dof_indices[i]);
              }

            // then move on to the next cell
            cell = next_cell;
          }
        else
          ++cell;

      // finally swap old and new content
      Assert(new_dof_indices.size() == new_size, ExcInternalError());
      dof_indices.swap(new_dof_indices);
      dof_offsets.swap(new_dof_offsets);
    }



    template <int dim, int spacedim>
    void
    DoFLevel::uncompress_data(
      const dealii::hp::FECollection<dim, spacedim> &fe_collection)
    {
      if (dof_offsets.size() == 0 || dof_indices.size() == 0)
        return;

      // in a first run through, count how many new slots we need in the
      // dof_indices array after uncompression.
      unsigned int new_size = 0;
      for (unsigned int cell = 0; cell < dof_offsets.size(); ++cell)
        if (dof_offsets[cell] != static_cast<offset_type>(-1))
          {
            // we know now that the slot for this cell is used. extract the
            // active_fe_index for it and see how many entries we need
            new_size += fe_collection[active_fe_index(cell)]
                          .template n_dofs_per_object<dim>();
          }

      // now allocate the new array and copy into it whatever we need
      std::vector<types::global_dof_index> new_dof_indices;
      new_dof_indices.reserve(new_size);
      std::vector<offset_type> new_dof_offsets(dof_offsets.size(),
                                               static_cast<offset_type>(-1));
      for (unsigned int cell = 0; cell < dof_offsets.size();)
        // see if this cell is active on the current level
        if (dof_offsets[cell] != static_cast<offset_type>(-1))
          {
            // find the next cell active on this level
            unsigned int next_cell = cell + 1;
            while ((next_cell < dof_offsets.size()) &&
                   (dof_offsets[next_cell] == static_cast<offset_type>(-1)))
              ++next_cell;

            const unsigned int next_offset =
              (next_cell < dof_offsets.size() ? dof_offsets[next_cell] :
                                                dof_indices.size());

            // set offset for this cell
            new_dof_offsets[cell] = new_dof_indices.size();

            // see if we need to uncompress this set of dofs
            if (is_compressed_entry(active_fe_indices[cell]) == false)
              {
                // apparently not. simply copy them
                Assert(next_offset - dof_offsets[cell] ==
                         fe_collection[active_fe_indices[cell]]
                           .template n_dofs_per_object<dim>(),
                       ExcInternalError());
                for (unsigned int i = dof_offsets[cell]; i < next_offset; ++i)
                  new_dof_indices.push_back(dof_indices[i]);
              }
            else
              {
                // apparently so. uncompress
                Assert(next_offset - dof_offsets[cell] == 1,
                       ExcInternalError());
                const unsigned int dofs_per_object =
                  fe_collection[get_toggled_compression_state(
                                  active_fe_indices[cell])]
                    .template n_dofs_per_object<dim>();
                for (unsigned int i = 0; i < dofs_per_object; ++i)
                  new_dof_indices.push_back(dof_indices[dof_offsets[cell]] + i);

                // then mark the uncompression
                active_fe_indices[cell] =
                  get_toggled_compression_state(active_fe_indices[cell]);
              }

            // then move on to the next cell
            cell = next_cell;
          }
        else
          ++cell;

      // verify correct size, then swap arrays
      Assert(new_dof_indices.size() == new_size, ExcInternalError());
      dof_indices.swap(new_dof_indices);
      dof_offsets.swap(new_dof_offsets);
    }



    std::size_t
    DoFLevel::memory_consumption() const
    {
      return (MemoryConsumption::memory_consumption(active_fe_indices) +
              MemoryConsumption::memory_consumption(dof_indices) +
              MemoryConsumption::memory_consumption(dof_offsets) +
              MemoryConsumption::memory_consumption(cell_cache_offsets) +
              MemoryConsumption::memory_consumption(cell_dof_indices_cache) +
              MemoryConsumption::memory_consumption(future_fe_indices));
    }



    void
    DoFLevel::normalize_active_fe_indices()
    {
      for (auto &active_fe_index : active_fe_indices)
        if (is_compressed_entry(active_fe_index))
          active_fe_index = get_toggled_compression_state(active_fe_index);
    }



    // explicit instantiations
    template void
    DoFLevel::compress_data(const dealii::hp::FECollection<1, 1> &);
    template void
    DoFLevel::compress_data(const dealii::hp::FECollection<1, 2> &);
    template void
    DoFLevel::compress_data(const dealii::hp::FECollection<1, 3> &);
    template void
    DoFLevel::compress_data(const dealii::hp::FECollection<2, 2> &);
    template void
    DoFLevel::compress_data(const dealii::hp::FECollection<2, 3> &);
    template void
    DoFLevel::compress_data(const dealii::hp::FECollection<3, 3> &);

    template void
    DoFLevel::uncompress_data(const dealii::hp::FECollection<1, 1> &);
    template void
    DoFLevel::uncompress_data(const dealii::hp::FECollection<1, 2> &);
    template void
    DoFLevel::uncompress_data(const dealii::hp::FECollection<1, 3> &);
    template void
    DoFLevel::uncompress_data(const dealii::hp::FECollection<2, 2> &);
    template void
    DoFLevel::uncompress_data(const dealii::hp::FECollection<2, 3> &);
    template void
    DoFLevel::uncompress_data(const dealii::hp::FECollection<3, 3> &);
  } // namespace hp
} // namespace internal

DEAL_II_NAMESPACE_CLOSE
