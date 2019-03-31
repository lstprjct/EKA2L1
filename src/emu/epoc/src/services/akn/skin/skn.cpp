/*
 * Copyright (c) 2019 EKA2L1 Team.
 * 
 * This file is part of EKA2L1 project.
 * 
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program. If not, see <http://www.gnu.org/licenses/>.
 */

#include <epoc/services/akn/skin/skn.h>
#include <common/log.h>

namespace eka2l1::epoc {
    skn_file::skn_file(common::ro_stream *stream)
        : master_chunk_size_(0),
          master_chunk_count_(0),
          crr_filename_id_(0),
          stream_(stream) {
        if (!read_master_chunk()) {
            LOG_ERROR("Reading master chunk failed!");
        }
    }

    bool skn_file::read_master_chunk() {
        if (stream_->read(skn_desc_dfo_common_len, &master_chunk_size_, 4) != 4) {
            return false;
        }

        std::uint16_t master_chunk_type = 0;

        if (stream_->read(skn_desc_dfo_common_type, &master_chunk_type, 2) != 2) {
            return false;
        }

        if (master_chunk_type != as_desc_skin_desc) {
            LOG_ERROR("Master chunk of file corrupted!");
            return false;
        }

        if (stream_->read(skn_desc_dfo_skin_chunks_count, &master_chunk_count_, 4) != 4) {
            return false;
        }

        process_chunks(skn_desc_dfo_skin_content, master_chunk_count_);

        return true;
    }

    bool skn_file::process_chunks(std::uint32_t base_offset, const std::int32_t count) {
        for (std::int32_t i = 0; i < count; i++) {
            std::uint32_t chunk_size = 0;
            std::uint16_t chunk_type = 0;

            if (stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4) != 4) {
                return false;
            }

            if (stream_->read(base_offset + skn_desc_dfo_common_type, &chunk_type, 2) != 2) {
                return false;
            }

            switch (chunk_type) {
            case as_desc_info: {
                base_offset += handle_info_chunk(base_offset, info_);
                break;
            }

            case as_desc_name: {
                base_offset += handle_name_chunk(base_offset, skin_name_);
                break;
            }

            case as_desc_filename: {
                base_offset += handle_filename_chunk(base_offset);
                break;
            }

            case as_desc_skin_desc_class: {
                base_offset += handle_class_chunk(base_offset);
                break;
            }

            default: {
                LOG_ERROR("Unhandled chunk type: {}", chunk_type);
                base_offset += chunk_size;

                break;
            }
            }
        }

        return true;
    }

    std::uint32_t skn_file::handle_info_chunk(std::uint32_t base_offset, skn_file_info &info) {
        std::uint32_t chunk_size = 0;

        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);
        stream_->read(base_offset + skn_desc_dfo_info_compiler_ver, &info.version, 4);

        std::uint16_t author_str_len = 0;
        stream_->read(base_offset + skn_desc_dfo_info_author_len, &author_str_len, 2);

        info.author.resize(author_str_len);

        author_str_len *= 2;
        stream_->read(base_offset + skn_desc_dfo_info_author_str, &info.author[0], author_str_len);
        
        std::uint16_t copyright_str_len = 0;
        stream_->read(base_offset + skn_desc_dfo_info_author_str + author_str_len, &copyright_str_len, 2);

        info.copyright.resize(copyright_str_len);

        copyright_str_len *= 2;
        stream_->read(base_offset + skn_desc_dfo_info_author_str + author_str_len + 2, &info.copyright[0], copyright_str_len);
        stream_->read(base_offset + skn_desc_dfo_info_author_str + author_str_len + 2 + copyright_str_len, &info.plat, 2);

        return chunk_size;
    }

    std::uint32_t skn_file::handle_name_chunk(std::uint32_t base_offset, skn_name &name) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);
        stream_->read(base_offset + skn_desc_dfo_name_lang, &name.lang, 2);
        
        std::uint16_t name_len = 0;
        stream_->read(base_offset + skn_desc_dfo_name_name_Len, &name_len, 2);

        name.name.resize(name_len);
        stream_->read(base_offset + skn_desc_dfo_name_name, &name.name[0], name_len * 2);

        return chunk_size;
    }

    std::uint32_t skn_file::handle_filename_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);
        
        std::int32_t id = 0;
        stream_->read(base_offset + skn_desc_dfo_filename_filename_id, &id, 4);

        id += crr_filename_id_;

        std::uint16_t strlen = 0;
        stream_->read(base_offset + skn_desc_dfo_filename_len, &strlen, 4);

        std::u16string name;
        name.resize(strlen);

        stream_->read(base_offset + skn_desc_dfo_filename_filename, &name[0], strlen * 2);

        filenames_.emplace(id, std::move(name));

        return chunk_size;
    }

    std::uint32_t skn_file::handle_class_chunk(std::uint32_t base_offset) {
        std::uint32_t chunk_size = 0;
        stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

        std::uint32_t count = 0;
        stream_->read(base_offset + skn_desc_dfo_class_chunk_n, &count, 4);

        process_class_def_chunks(base_offset + skn_desc_dfo_class_content, count);

        return chunk_size;
    }
    
    void skn_file::process_class_def_chunks(std::uint32_t base_offset, std::int32_t chunk_count) {
        for (std::int32_t i = 0; i < chunk_count; i++) {
            std::uint32_t chunk_size = 0;
            stream_->read(base_offset + skn_desc_dfo_common_len, &chunk_size, 4);

            std::uint16_t chunk_type = 0;
            stream_->read(base_offset + skn_desc_dfo_common_type, &chunk_type, 2);

            switch (chunk_type) {
            case as_desc_skin_desc_bmp_item_def: {
                process_bitmap_def_chunk(base_offset);
                break;
            }

            default: {
                LOG_ERROR("Unknown class def chunk type: {}", chunk_type);
                break;
            }
            }
        
            base_offset += chunk_size;
        }
    }

    void skn_file::process_bitmap_def_chunk(std::uint32_t base_offset) {
        skn_bitmap_info bmp_info_ {};
        bmp_info_.type = skn_def_type::bitmap;
        
        stream_->read(base_offset + skn_desc_dfo_bitmap_hash_id, &bmp_info_.id_hash, 8);
        stream_->read(base_offset + skn_desc_dfo_bitmap_filename_id, &bmp_info_.filename_id, 4);
        stream_->read(base_offset + skn_desc_dfo_bitmap_bitmap_idx, &bmp_info_.bmp_idx, 4);
        stream_->read(base_offset + skn_desc_dfo_bitmap_mask_idx, &bmp_info_.mask_bitmap_idx, 4);

        process_attrib(base_offset + skn_desc_dfo_bitmap_attribs, bmp_info_.attrib);

        bitmaps_.emplace(bmp_info_.id_hash, std::move(bmp_info_));
    }

    void skn_file::process_attrib(std::uint32_t base_offset, skn_attrib_info &attrib) {
        std::uint16_t ver = 0;
        stream_->read(base_offset + skn_desc_dfo_common_ver, &ver, 2);

        std::uint16_t attrib_and_align = 0;
        stream_->read(base_offset + skn_desc_dfo_attribs_alignment, &attrib_and_align, 2);

        attrib.attrib = attrib_and_align & 0xFF;
        attrib.align = (attrib_and_align & 0xFF00) >> 8;

        stream_->read(base_offset + skn_desc_dfo_attribs_coordx, &attrib.image_coord_x, 2);
        stream_->read(base_offset + skn_desc_dfo_attribs_coordy, &attrib.image_coord_y, 2);
        stream_->read(base_offset + skn_desc_dfo_attribs_sizew, &attrib.image_size_x, 2);
        stream_->read(base_offset + skn_desc_dfo_attribs_sizeh, &attrib.image_size_y, 2);

        if (ver >= 2) {
            // 16-bit flag
            stream_->read(base_offset + skn_desc_dfo_attribs_ext_attrib_flags, &attrib_and_align, 2);
            attrib.attrib |= attrib_and_align << 8;
        }
    }
}
