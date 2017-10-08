#include <boost/filesystem.hpp>
#include <xxhash.h>
#include <iostream>
#include <fstream>
#include <helpers.hpp>
#include <file/util.hpp>
#include "timer.hpp"
#include "bloom_filter.hpp"
#include "kmer.hpp"

namespace genome {

    void bloom_filter::create_hashes(const void* input, size_t len, size_t bloom_filter_size, size_t num_hashes,
                                     const std::function<void(size_t)>& callback) {
        for (unsigned int i = 0; i < num_hashes; i++) {
            size_t hash = XXH32(input, len, i);
            callback(hash % bloom_filter_size);
        }
    }

    void bloom_filter::process(const std::vector<boost::filesystem::path>& paths, const boost::filesystem::path& out_file, timer& t) {
        sample<31> s;
        for (size_t i = 0; i < paths.size(); i++) {
            t.active("read");
            file::deserialize(paths[i], s);
            t.active("process");
            for (const auto& kmer: s.data()) {
                create_hashes(&kmer, kmer.size, m_bloom_filter_size, m_num_hashes, [&](size_t hash) {
                    bloom_filter::set_bit(hash, i);
                });
            }
        }
        t.active("write");
        std::vector<std::string> file_names(8 * m_block_size);
        std::transform(paths.begin(), paths.end(), file_names.begin(), [](const auto& p) {
            return p.stem().string();
        });
        file::serialize(out_file, *this, file_names);
        t.stop();
    }

    void bloom_filter::create_from_samples(const boost::filesystem::path &in_dir, const boost::filesystem::path &out_dir,
                                                size_t bloom_filter_size, size_t block_size, size_t num_hashes) {
        timer t;
        bloom_filter bf(bloom_filter_size, block_size, num_hashes);
        bulk_process_files(in_dir, out_dir, 8 * block_size, file::sample_header::file_extension, file::bloom_filter_header::file_extension,
                           [&](const std::vector<boost::filesystem::path>& paths, const boost::filesystem::path& out_file) {
                               bf.process(paths, out_file, t);
                           });
        std::cout << t;
    }


    void bloom_filter::combine(std::vector<std::pair<std::ifstream, size_t>>& ifstreams, const boost::filesystem::path& out_file,
                               size_t bloom_filter_size, size_t block_size, size_t num_hash, timer& t, const std::vector<std::string>& file_names) {
        std::ofstream ofs;
        file::bloom_filter_header bfh(bloom_filter_size, block_size, num_hash, file_names);
        file::serialize_header(ofs, out_file, bfh);

        std::vector<char> block(block_size);
        for (size_t i = 0; i < bloom_filter_size; i++) {
            size_t pos = 0;
            t.active("read");
            for (auto& ifs: ifstreams) {
                ifs.first.read(&(*(block.begin() + pos)), ifs.second);
                pos += ifs.second;
            }
            t.active("write");
            ofs.write(&(*block.begin()), block_size);
        }
        t.stop();
    }

    void bloom_filter::combine_bloom_filters(const boost::filesystem::path& in_dir, const boost::filesystem::path& out_dir,
                                             size_t bloom_filter_size, size_t num_hashes, size_t batch_size) {
        timer t;
        std::vector<std::pair<std::ifstream, size_t>> ifstreams;
        std::vector<std::string> file_names;
        bulk_process_files(in_dir, out_dir, batch_size, file::bloom_filter_header::file_extension, file::bloom_filter_header::file_extension,
                           [&](const std::vector<boost::filesystem::path>& paths, const boost::filesystem::path& out_file) {
                               size_t new_block_size = 0;
                               for (const auto& p: paths) {
                                   ifstreams.emplace_back(std::make_pair(std::ifstream(), 0));
                                   auto bfh = file::deserialize_header<file::bloom_filter_header>(ifstreams.back().first, p);
                                   assert(bfh.bloom_filter_size() == bloom_filter_size);
                                   assert(bfh.num_hashes() == num_hashes);
                                   ifstreams.back().second = bfh.block_size();
                                   new_block_size += bfh.block_size();
                                   std::copy(bfh.file_names().begin(), bfh.file_names().end(), std::back_inserter(file_names));
                               }
                               combine(ifstreams, out_file, bloom_filter_size, new_block_size, num_hashes, t, file_names);
                               ifstreams.clear();
                               file_names.clear();
                           });
        std::cout << t;
    }

    bloom_filter::bloom_filter(uint64_t bloom_filter_size, uint64_t block_size, uint64_t num_hashes)
            : m_bloom_filter_size(bloom_filter_size), m_block_size(block_size), m_num_hashes(num_hashes),
              m_data(bloom_filter_size * block_size) {}

    void bloom_filter::set_bit(size_t pos, size_t bit_in_block) {
        m_data[m_block_size * pos + bit_in_block / 8] |= 1 << (bit_in_block % 8);
    }

    bool bloom_filter::is_set(size_t pos, size_t bit_in_block) {
        byte b = m_data[m_block_size * pos + bit_in_block / 8];
        return (b & (1 << (bit_in_block % 8))) != 0;
    };
    bool bloom_filter::contains(const kmer<31>& kmer, size_t bit_in_block) {
        assert(bit_in_block < 8 * m_block_size);
        for (unsigned int k = 0; k < m_num_hashes; k++) {
            size_t hash = XXH32(&kmer.data(), 8, k);
            if (!is_set(hash % m_bloom_filter_size, bit_in_block)) {
                return false;
            }
        }
        return true;
    }

    uint64_t bloom_filter::bloom_filter_size() const {
        return m_bloom_filter_size;
    }

    void bloom_filter::bloom_filter_size(uint64_t bloom_filter_size) {
        m_bloom_filter_size = bloom_filter_size;
    }

    uint64_t bloom_filter::block_size() const {
        return m_block_size;
    }

    void bloom_filter::block_size(uint64_t block_size) {
        m_block_size = block_size;
    }

    uint64_t bloom_filter::num_hashes() const {
        return m_num_hashes;
    }

    void bloom_filter::num_hashes(uint64_t num_hashes) {
        m_num_hashes = num_hashes;
    }

    const std::vector<byte>& bloom_filter::data() const {
        return m_data;
    }

    std::vector<byte>& bloom_filter::data() {
        return m_data;
    }
};
