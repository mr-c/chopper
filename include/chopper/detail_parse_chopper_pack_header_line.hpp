#pragma once

#include <seqan3/std/charconv>
#include <seqan3/std/algorithm>
#include <string>

#include <chopper/build/build_data.hpp>
#include <chopper/detail_bin_prefixes.hpp>

void parse_chopper_pack_header_line(std::string const & line, build_data & data)
{
    if (line.substr(1, hibf_prefix.size()) == hibf_prefix)
    {
        assert(line.substr(hibf_prefix.size() + 2, 11) == "max_bin_id:");
        auto it = std::find(line.begin() + hibf_prefix.size() + 13, line.end(), '_'); // skip "MERGED"/"SPLIT"
        ++it; // skip "_"
        it = std::find(it, line.end(), '_'); // skip "BIN"
        ++it; // skip "_"
        data.hibf_max_bin = std::atoi(std::string(it, line.end()).c_str());
    }
    else if (line.substr(1, merged_bin_prefix.size()) == merged_bin_prefix)
    {
        std::string const hidx_str(line.begin() + 1 /*#*/ + merged_bin_prefix.size() + 1 /*_*/,
                                   std::find(line.begin() + merged_bin_prefix.size() + 2, line.end(), ' '));
        assert(line.substr(merged_bin_prefix.size() + hidx_str.size() + 3, 11) == "max_bin_id:");
        std::string const lidx_str = line.substr(merged_bin_prefix.size() + hidx_str.size() + 14,
                                                 line.size() - merged_bin_prefix.size() - hidx_str.size() - 14);

        size_t const hidx = std::atoi(hidx_str.c_str());
        size_t const lidx = std::atoi(lidx_str.c_str());

        data.merged_max_bin_map.emplace(hidx, lidx);
    }
}

// [root node, high-level node, ]

auto parse_bin_indices(std::string const & str)
{
    std::vector<size_t> result;

    char const * buffer = str.c_str();
    auto start = &buffer[0];
    auto buffer_end = start + str.size();

    size_t tmp;
    while (start < buffer_end)
    {
        auto res = std::from_chars(start, buffer_end, tmp); // {false, pointer} 123
        start = res.ptr;
        ++start; // skip ;
        result.push_back(tmp);
    }

    return result;
}

void parse_chopper_pack_header(lemon::ListDigraph & ibf_graph,
                               lemon::ListDigraph::NodeMap<node_data> & node_map,
                               std::istream & chopper_pack_file)
{
    std::string line;
    std::getline(chopper_pack_file, line); // read first line
    assert(line[0] == '#'); // we are reading header lines
    assert(line.substr(1, hibf_prefix.size()) == hibf_prefix); // first line should always be High level IBF

    // parse High Level max bin index
    assert(line.substr(hibf_prefix.size() + 2, 11) == "max_bin_id:");
    std::string hibf_max_bin_str(line.begin() + 27, line.end());

    auto high_level_node = ibf_graph.addNode(); // high-level node = root node
    node_map.set(high_level_node, {0, parse_bin_indices(hibf_max_bin_str).front(), 0, lemon::INVALID, {}});

    std::vector<std::pair<std::vector<size_t>, size_t>> header_records{};

    // first read and parse header records, in order to sort them before adding them to the graph
    while (std::getline(chopper_pack_file, line) && line.substr(0, 6) != "#FILES")
    {
        assert(line.substr(1, merged_bin_prefix.size()) == merged_bin_prefix);

        // parse header line
        std::string const indices_str(line.begin() + 1 /*#*/ + merged_bin_prefix.size() + 1 /*_*/,
                                      std::find(line.begin() + merged_bin_prefix.size() + 2, line.end(), ' '));

        assert(line.substr(merged_bin_prefix.size() + indices_str.size() + 3, 11) == "max_bin_id:");
        std::string const max_id_str = line.substr(merged_bin_prefix.size() + indices_str.size() + 14,
                                                   line.size() - merged_bin_prefix.size() - indices_str.size() - 14);

        std::vector<size_t> const bin_indices = parse_bin_indices(indices_str);
        size_t const max_id = parse_bin_indices(max_id_str).front();

        header_records.emplace_back(std::move(bin_indices), max_id);
    }

    // sort records ascending by the number of bin indices (corresponds to the IBF levels)
    std::ranges::sort(header_records, [] (auto const & r, auto const & l) { return r.first.size() < l.first.size(); });

    for (auto const & [bin_indices, max_id] : header_records)
    {
        // we assume that the header lines are in the correct oder (TODO order them)
        // go down the tree until you find the matching parent
        auto it = bin_indices.begin();
        lemon::ListDigraph::Node current_node = high_level_node; // start at root

        while (it != (bin_indices.end() - 1))
        {
            for (lemon::ListDigraph::OutArcIt arc_it(ibf_graph, current_node); arc_it != lemon::INVALID; ++arc_it)
            {
                auto target = ibf_graph.target(arc_it);
                if (node_map[target].parent_bin_index == *it)
                {
                    current_node = target;
                    break;
                }
            }
            ++it;
        }

        auto new_node = ibf_graph.addNode();
        ibf_graph.addArc(current_node, new_node);
        node_map.set(new_node, {bin_indices.back(), max_id, 0, lemon::INVALID, {}});

        if (node_map[current_node].max_bin_index == bin_indices.back())
            node_map[current_node].favourite_child = new_node;
    }
}
