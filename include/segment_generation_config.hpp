#pragma once

struct map_distance_matrix : std::unordered_map<size_t, double>
{
    // i = first = row , j = second = column
    // 0 1 2 3 4     i * nseq + j
    // 5 6 7 8 9
    //
    using base_t = std::unordered_map<size_t, double>;
    using value_type = double;
    using size_type = size_t;
    using reference = value_type;

    // number of sequences stored in this matrix
    size_type nseq{};

    double dummy{1.0}; // dummy distance is always maximum distance

    reference & operator[](size_type index)
    {
        dummy = 1.0; // reset in case it was changed
        auto it = this->find(index);

        if (it == this->end())
            return dummy;
        else
            return it->second;
    }
};

namespace seqan
{

auto length(map_distance_matrix const & m)
{
    return m.nseq * m.nseq;
}

} // namespace seqan3

template <typename TSize>
struct segment_generation_config
{
    std::vector<std::string> seqfiles;
    std::string output_graph_file{"graph.dot"}; // default

    uint8_t kmer_size{25};
    uint16_t window_size{100};

    seqan::String<TSize> global_alignment_pairs;
    seqan::String<TSize> local_alignment_pairs;
    seqan::String<TSize> semi_global_alignment_pairs;

    map_distance_matrix distanceMatrix;
};
