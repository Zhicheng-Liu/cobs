/*******************************************************************************
 * cobs/query/classic_search.hpp
 *
 * Copyright (c) 2018 Florian Gauger
 * Copyright (c) 2018 Timo Bingmann
 *
 * All rights reserved. Published under the MIT License in the LICENSE file.
 ******************************************************************************/

#ifndef COBS_QUERY_CLASSIC_SEARCH_HEADER
#define COBS_QUERY_CLASSIC_SEARCH_HEADER

#include <cobs/query/index_file.hpp>
#include <cobs/query/search.hpp>
#include <cobs/util/query.hpp>

namespace cobs {

class ClassicSearch : public Search
{
public:
    //! method to try to auto-detect and load IndexSearchFile
    ClassicSearch(std::string path);

    ClassicSearch(std::shared_ptr<IndexSearchFile> index);

    ClassicSearch(std::vector<std::shared_ptr<IndexSearchFile> > indices);

    void search(
        const std::string& query,
        std::vector<std::pair<uint16_t, std::string> >& result,
        double threshold = 0.0, size_t num_results = 0) final;

protected:
    //! reference to index file query object to retrieve data
    std::vector<std::shared_ptr<IndexSearchFile> > index_files_;
};

} // namespace cobs

#endif // !COBS_QUERY_CLASSIC_SEARCH_HEADER

/******************************************************************************/
