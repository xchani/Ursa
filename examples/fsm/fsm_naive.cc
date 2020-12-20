// Copyright 2018 H-AXE
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
// http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

#include <cctype>
#include <memory>
#include <string>
#include <utility>
#include <vector>
#include <stack>

#include "glog/logging.h"

#include "base/tokenizer.h"
#include "common/engine.h"

using axe::base::Tokenizer;

using LabelType = int;

class Neighbor
{
public:
    Neighbor() {}
    Neighbor(int e_id, int v_id, int e_label) : v_id_(v_id), e_id_(e_id), e_label_(e_label) {}

    int GetVId() const { return v_id_; }
    int GetEId() const { return e_id_; }
    int GetELabel() const { return e_label_; }

    bool operator<(const Neighbor &other) const { return e_id_ < other.e_id_; }
    bool operator==(const Neighbor &other) const { return e_id_ == other.e_id_; }

    friend void operator<<(axe::base::BinStream &bin_stream, const Neighbor &n) { bin_stream << n.e_id_ << n.v_id_ << n.e_label_; }
    friend void operator>>(axe::base::BinStream &bin_stream, Neighbor &n) { bin_stream >> n.e_id_ >> n.v_id_ >> n.e_label_; }

private:
    int e_id_;
    int v_id_;
    int e_label_;
};

class Vertex
{
public:
    Vertex() : neighbors_(std::make_shared<std::vector<Neighbor>>()) {}
    Vertex(int id, int label,
           const std::shared_ptr<std::vector<Neighbor>> &neighbors) : id_(id), label_(label), neighbors_(neighbors) {}

    int GetId() const { return id_; }
    int GetLabel() const { return label_; }
    const std::shared_ptr<std::vector<Neighbor>> &GetNeighbors() const { return neighbors_; }
    int GetNeighborCount() const { return neighbors_->size(); }

    void SetId(int id) { id_ = id; }
    void SetLabel(const int &label) { label_ = label; }
    void AddNeighbor(const Neighbor &neighbor) { neighbors_->push_back(neighbor); }

    double GetMemory() const
    {
        double ret = sizeof(int) + neighbors_->size() * sizeof(int);
        return ret;
    }
    bool operator<(const Vertex &other) const { return id_ < other.id_; }
    bool operator==(const Vertex &other) const { return id_ == other.id_; }

    friend void operator<<(axe::base::BinStream &bin_stream, const Vertex &v) { bin_stream << v.id_ << v.label_ << *(v.neighbors_); }
    friend void operator>>(axe::base::BinStream &bin_stream, Vertex &v) { bin_stream >> v.id_ >> v.label_ >> *(v.neighbors_); }

private:
    int id_;
    int label_;
    std::shared_ptr<std::vector<Neighbor>> neighbors_; // store the vetex.id of neighbors.
};

class Graph
{
public:
    Graph() : vertices_(std::make_shared<std::vector<Vertex>>()) {}
    Graph(int id, int support,
          const std::shared_ptr<std::vector<Vertex>> &vertices) : id_(id), support_(support), vertices_(vertices) {}
    Graph(const Graph &graph)
    {
        this->id_ = graph.id_;
        this->support_ = graph.support_;
        this->vertices_ = std::make_shared<std::vector<Vertex>>(*graph.vertices_);
    }

    int GetId() const { return id_; }
    int GetSupport() const { return support_; }
    void SetId(int id) { id_ = id; }
    void SetSupport(int support) { support_ = support; }
    const std::shared_ptr<std::vector<Vertex>> &GetVertices() const { return vertices_; }
    void AddVertex(const Vertex &vertex) { vertices_->push_back(vertex); }

    bool operator<(const Graph &other) const { return id_ < other.id_; }
    bool operator==(const Graph &other) const { return id_ == other.id_; }

    friend void operator<<(axe::base::BinStream &bin_stream, const Graph &g) { bin_stream << g.id_ << g.support_ << *(g.vertices_); }
    friend void operator>>(axe::base::BinStream &bin_stream, Graph &g) { bin_stream >> g.id_ >> g.support_ >> *(g.vertices_); }

private:
    int id_;
    int support_;
    std::shared_ptr<std::vector<Vertex>> vertices_;
};

int ReadInt(const std::string &line, size_t &ptr)
{
    int ret = 0;
    while (ptr < line.size() && !isdigit(line.at(ptr)))
        ++ptr;
    CHECK(ptr < line.size()) << "Invalid Input";
    while (ptr < line.size() && isdigit(line.at(ptr)))
    {
        ret = ret * 10 + line.at(ptr) - '0';
        ++ptr;
    }
    return ret;
};

DatasetPartition<Graph> ParseLineGraph(const std::string &line)
{
    Graph g;
    size_t ptr = 0;
    Vertex vertex;
    int v_id = ReadInt(line, ptr);
    int v_label = ReadInt(line, ptr);
    vertex.SetId(v_id);
    vertex.SetLabel(v_label);
    int neighbor_count = ReadInt(line, ptr);
    for (int i = 0; i < neighbor_count; i++)
    {
        int e_id = 0;
        int e_to = ReadInt(line, ptr);
        int e_label = ReadInt(line, ptr);
        Neighbor neighbor(e_id, e_to, e_label);
        vertex.AddNeighbor(neighbor);
    }
    g.AddVertex(vertex);
    DatasetPartition<Graph> ret;
    ret.push_back(g);
    return ret;
};

DatasetPartition<Vertex> ParseLineVertex(const std::string &line)
{
    size_t ptr = 0;
    Vertex vertex;
    int v_id = ReadInt(line, ptr);
    int v_label = ReadInt(line, ptr);
    vertex.SetId(v_id);
    vertex.SetLabel(v_label);
    int neighbor_count = ReadInt(line, ptr);
    for (int i = 0; i < neighbor_count; i++)
    {
        int e_id = 0;
        int e_to = ReadInt(line, ptr);
        int e_label = ReadInt(line, ptr);
        Neighbor neighbor(e_id, e_to, e_label);
        vertex.AddNeighbor(neighbor);
    }
    DatasetPartition<Vertex> ret;
    ret.push_back(vertex);
    return ret;
};

class FSMNaive : public Job
{
public:
    void Run(TaskGraph *tg, const std::shared_ptr<Properties> &config) const override
    {
        auto input = config->GetOrSet("graph", "/home/ckchang/software/ck-ursa/examples/dataset/test1.lg");
        int n_partitions = std::stoi(config->GetOrSet("parallelism", "1"));
        int minimal_support = std::stoi(config->GetOrSet("minimal_support", "5"));
        int n_iters = std::stoi(config->GetOrSet("n_iters", "5")); // the max number of edges in frequent subgraph.
        // means the graph is composed by a list of vertex.
        auto graph = TextSourceDataset(input, tg, n_partitions)
                         .FlatMap([](const std::string &line) { return ParseLineVertex(line); },
                                  [](const std::vector<double> &input) {
                                      double ret = 0;
                                      for (double x : input)
                                      {
                                          ret += x;
                                      }
                                      return ret * 2;
                                  })
                         .PartitionBy([](const Vertex &v) { return v.GetId(); }, n_partitions);
        // graph.UpdatePartition([](DatasetPartition<Vertex> &data) {
        //     std::sort(data.begin(), data.end(), [](const Vertex &a, const Vertex &b) { return a.GetId() < b.GetId(); });
        // }); // If n_partitions=1, in fact this is not needed as the data is sorted by default.
        auto shared_graph = std::make_shared<axe::common::Dataset<Vertex>>(graph);

        auto graphs = graph.MapPartition([](const DatasetPartition<Vertex> &data) {
            DatasetPartition<Graph> ret;
            auto vertices = std::make_shared<std::vector<Vertex>>();
            for (auto &v : data)
                vertices->push_back(v);
            ret.push_back(Graph(0, 1, vertices));
            return ret;
        });
        auto shared_graphs = std::make_shared<axe::common::Dataset<Graph>>(graphs); // The big single graph G , with id=0, and support=1.
        // auto shared_graphs = std::make_shared<axe::common::Dataset<Graph>>(graph.MapPartition([](const DatasetPartition<Vertex> &data) {
        //     DatasetPartition<Graph> ret;
        //     ret.push_back(Graph(0, 1, data)); // The big single graph G , with id=0, and support=1.
        //     return ret;
        // }));

        auto start_candidates_label = graph.MapPartition([](const DatasetPartition<Vertex> &data) {
                                               DatasetPartition<std::pair<LabelType, int>> ret;
                                               for (const auto &v : data)
                                                   ret.push_back(std::make_pair(v.GetLabel(), 1));
                                               return ret;
                                           })
                                          .ReduceBy([](const std::pair<LabelType, int> v_c) { return v_c.first; },
                                                    [](std::pair<LabelType, int> &agg, const std::pair<LabelType, int> &update) { agg.second += update.second; })
                                          .MapPartition([minimal_support](const DatasetPartition<std::pair<LabelType, int>> &data) {
                                              DatasetPartition<std::pair<LabelType, int>> ret;
                                              for (const auto &label_count : data)
                                              {
                                                  if (label_count.second >= minimal_support)
                                                  {
                                                      ret.push_back(label_count);
                                                  }
                                              }
                                              return ret;
                                          })
                                          .PartitionBy([](const std::pair<LabelType, int> &) { return 0; }, 1);

        // auto start_vertices = start_candidates_label.MapPartition([](const DatasetPartition<std::pair<int, int>> data) {
        //     DatasetPartition<std::pair<Vertex, int>> ret;
        //     for (int i = 0; i < data.size(); i++)
        //     {
        //         Vertex vertex;
        //         vertex.SetId(i); // this is a subgraph, or pattern to identify.
        //         vertex.SetLabel(data.at(i).first);
        //         ret.push_back(std::make_pair(vertex, data.at(i).second));
        //     }
        //     return ret;
        // });

        // auto candidates = std::make_shared<axe::common::Dataset<Graph>>(start_vertices.MapPartition([](const DatasetPartition<std::pair<Vertex, int>> &data) {
        //     DatasetPartition<Graph> ret;
        //     for (int i = 0; i < data.size(); i++)
        //     {
        //         ret.push_back(Graph(i, data.at(i).second, data.at(i).first));
        //     }
        //     return ret;
        // }));
        // auto result = candidates;

        // auto extend_subgraph = [](const DatasetPartition<Graph> &subgraph, const DatasetPartition<Graph> &src_graph) {

        // };
        // // main loop
        // for (int i = 0; i < n_iters; ++i)
        // {

        // }

        auto get_graphs_with_vertex_label = [](const DatasetPartition<std::pair<int, int>> &labels, const DatasetPartition<Vertex> &src_graph) {
            DatasetPartition<Graph> ret;
            for (const auto &v : src_graph)
            {
                for (const auto &label : labels)
                {
                    if (label.first == v.GetLabel())
                    {
                        Vertex new_vertex;
                        new_vertex.SetId(v.GetId());
                        new_vertex.SetLabel(v.GetLabel());

                        auto vertices = std::make_shared<std::vector<Vertex>>();
                        vertices->push_back(new_vertex);
                        ret.push_back(Graph(0, label.second, vertices));
                        break;
                    }
                }
            }
            return ret;
        };
        auto start_candidates = start_candidates_label.SharedDataMapPartitionWith(shared_graph.get(), get_graphs_with_vertex_label);
        auto results = start_candidates;
        auto extend_subgraph = [](const DatasetPartition<Graph> &subgraphs, const DatasetPartition<Graph> &src_graph) {
            DatasetPartition<Graph> extended_subgraphs;
            auto src_vertices = *src_graph.at(0).GetVertices();
            for (const auto &subgraph : subgraphs)
            {
                // For each candidate subgraph, try to get a lot of extended graph from it.

                std::vector<int> sg_vids; // The vertex Id of all the vertices in the subgraph.
                for (const auto &sg_vertex : *subgraph.GetVertices())
                {
                    sg_vids.push_back(sg_vertex.GetId());
                }

                int sg_v_i = 0;
                for (const auto &sg_vertex : *subgraph.GetVertices())
                {
                    // We extend a subgraph based on its vertex by adding edge on vertex.
                    const auto &src_vertex = src_vertices.at(sg_vertex.GetId());
                    if (sg_vertex.GetNeighborCount() < src_vertex.GetNeighborCount())
                    {
                        // If the neighbour count of the subgraph-vertex is less than src vertex,
                        // that means we can extend a edge on subgraph-vertex, and then get a new candidate.
                        for (const auto &src_neighbor : *src_vertex.GetNeighbors())
                        {
                            bool edge_existed = false;
                            // If the edge in src_vertex is existing in subgraph, then we go on to next edge.
                            for (const auto &sg_neighbor : *sg_vertex.GetNeighbors())
                            {
                                if (sg_neighbor.GetELabel() == src_neighbor.GetELabel() && sg_neighbor.GetVId() == src_neighbor.GetVId())
                                {
                                    edge_existed = true;
                                    break;
                                }
                            }
                            if (!edge_existed)
                            {
                                // We can add this edge in src_vertex to subgraph.
                                auto new_sg_vertices = std::make_shared<std::vector<Vertex>>();
                                Graph new_graph(0, 1, new_sg_vertices);
                                (*new_graph.GetVertices()) = *subgraph.GetVertices(); // should be a deep copy.

                                (*new_graph.GetVertices()).at(sg_v_i).AddNeighbor(src_neighbor); // Add the new edge to the new_subgraph.
                                CHECK_EQ((*new_graph.GetVertices()).at(sg_v_i).GetNeighborCount(), (*subgraph.GetVertices()).at(sg_v_i).GetNeighborCount() + 1);

                                // If the neighbor vertex of this edge doesn't exist in the subgraph, add it.
                                bool neigher_existed = false;
                                for (size_t i = 0; i < sg_vids.size(); i++)
                                {
                                    if (sg_vids[i] == src_neighbor.GetVId())
                                    {
                                        neigher_existed = true;
                                        break;
                                    }
                                }
                                if (!neigher_existed)
                                {
                                    Vertex new_neighbor_vertex;
                                    new_neighbor_vertex.SetId(src_neighbor.GetVId());
                                    new_neighbor_vertex.SetLabel(src_vertices.at(src_neighbor.GetVId()).GetLabel());
                                    new_neighbor_vertex.AddNeighbor(Neighbor(src_neighbor.GetEId(), sg_v_i, src_neighbor.GetELabel()));

                                    new_graph.AddVertex(new_neighbor_vertex);
                                }

                                extended_subgraphs.push_back(new_graph);
                            }
                        }
                    }
                    sg_v_i++;
                }
            }
            return extended_subgraphs;
        };
        auto frequent_subgraph_isomorphism = [](const DatasetPartition<Graph> &subgraphs, const DatasetPartition<Graph> &src_graph) {
            // subgraph: a lot of candidate subgraphs
            // src_graph: the original big graph
            // progress: iterate over each subgraph, calculate its support in src_graph
            // return: the candidate that support is over minimal_support.
            DatasetPartition<Graph> frequent_subgraphs;
            int src_support = src_graph.at(0).GetSupport();
            for (const auto &subgraph : subgraphs)
            {
                // Prepare
                size_t subgraph_size = subgraph.GetVertices()->size();
                // from/to id and edge in subgraph
                std::map<std::pair<int, int>, int> sg_edge_labels;
                for (const auto &sg_vertex : *subgraph.GetVertices())
                {
                    for (const auto &sg_neighbor : *sg_vertex.GetNeighbors())
                    {
                        auto key = std::make_pair(sg_vertex.GetId(), sg_neighbor.GetVId());
                        sg_edge_labels[key] = sg_neighbor.GetELabel();
                    }
                }

                // Find all possible mapping vertices in big graph
                std::map<Vertex, std::vector<Vertex>> vertices_mapping;
                for (const auto &sg_vertex : *subgraph.GetVertices())
                {
                    for (const auto &src_vertex : *src_graph.at(0).GetVertices())
                    {
                        // If they have same vertex label, they can be mapped
                        if (sg_vertex.GetLabel() == src_vertex.GetLabel())
                        {
                            vertices_mapping[sg_vertex].push_back(src_vertex);
                        }
                    }
                }
                // Turn map to vector for sequential access
                // TODO(Chenxia): remove raw pointer
                std::vector<std::pair<Vertex, std::vector<Vertex>* >> vertices_mapping_vec;
                for (auto &vertex_mapping : vertices_mapping)
                {
                    vertices_mapping_vec.push_back(
                        std::make_pair(vertex_mapping.first, &(vertex_mapping.second)));
                }
                // Check if number of all possible subgraphs not less than support
                long long cand_subgraphs_num = 1;
                for (const auto &vertex_mapping : vertices_mapping)
                {
                    cand_subgraphs_num *= vertex_mapping.second.size();
                }
                // TODO(Chenxia): potential bug when it's beyond the range of int
                if (cand_subgraphs_num < src_support)
                {
                    return frequent_subgraphs;
                }
                // Enumerate all possible graphs and its mapping
                std::stack<std::pair<std::shared_ptr<Graph>, std::shared_ptr<std::map<int, int>>>> cand_subgraphs;
                // Add first possible vertices to stack
                auto first_sg_vertex = vertices_mapping_vec.at(0).first;
                auto first_src_vertices = vertices_mapping_vec.at(0).second;
                for (const auto &first_src_vertex : *first_src_vertices)
                {
                    auto cand_subgraph = std::make_shared<Graph>();
                    auto cand_map = std::make_shared<std::map<int, int>>();

                    // subgraph of current candidate
                    cand_subgraph->AddVertex(first_src_vertex);

                    // map for current candidate subgraph
                    cand_map->insert({first_sg_vertex.GetId(), first_src_vertex.GetId()});

                    // add candidate to stack
                    cand_subgraphs.push(std::make_pair(cand_subgraph, cand_map));
                }
                // Enumerate next possible subgraphs
                while (!cand_subgraphs.empty())
                {
                    auto tmp_cand = cand_subgraphs.top();
                    cand_subgraphs.pop();
                    auto cand_subgraph = tmp_cand.first;
                    auto cand_map = tmp_cand.second;
                    // auto [cand_subgraph, cand_map] = cand_subgraphs.pop();
                    auto cand_subgraph_size = cand_subgraph->GetVertices()->size();

                    auto sg_vertex = vertices_mapping_vec.at(cand_subgraph_size).first;
                    auto src_vertices = vertices_mapping_vec.at(cand_subgraph_size).second;
                    for (const auto &src_vertex : *src_vertices)
                    {
                        // check whether the vertex is already in the candidate subgraph
                        bool duplicate_vertex = false;
                        std::set<int> cand_vid_set;
                        for (const auto &cand_vertex : *cand_subgraph->GetVertices())
                        {
                            cand_vid_set.insert(cand_vertex.GetId());
                            if (src_vertex == cand_vertex)
                            {
                                duplicate_vertex = true;
                                break;
                            }
                        }
                        if (duplicate_vertex)
                        {
                            continue;
                        }

                        auto new_cand_subgraph = std::make_shared<Graph>(*cand_subgraph);
                        auto new_cand_map = std::make_shared<std::map<int, int>>(*cand_map);

                        new_cand_subgraph->AddVertex(src_vertex);
                        new_cand_map->insert({sg_vertex.GetId(), src_vertex.GetId()});

                        if (cand_subgraph_size + 1 == subgraph_size)
                        {
                            // check if every pair of vertices have same edges and labels
                            bool isomorphism = true;

                            // from/to id and edge in candidate subgraph
                            std::map<std::pair<int, int>, int> src_edge_labels;
                            for (const auto &src_vertex : *new_cand_subgraph->GetVertices())
                            {
                                for (const auto &src_neighbor : *src_vertex.GetNeighbors())
                                {
                                    auto key = std::make_pair(src_vertex.GetId(), src_neighbor.GetVId());
                                    src_edge_labels[key] = src_neighbor.GetELabel();
                                }
                            }

                            // enumerate all pairs of vertices in subgraph
                            for (const auto &sg_vertex_from : *subgraph.GetVertices())
                            {
                                for (const auto &sg_vertex_to : *subgraph.GetVertices())
                                {
                                    // if (sg_vertex_from == sg_vertex_to)
                                    // {
                                    //     continue;
                                    // }

                                    // mapping vertex in candidate subgraph
				    for (const auto &it : *new_cand_map) {
					LOG(INFO) << "sg id: " << it.first << ", src id: " << it.second << std::endl;
				    }
				    LOG(INFO) << "# sg from id: " << sg_vertex_from.GetId() << std::endl;
                                    auto src_vertex_from_id = new_cand_map->at(sg_vertex_from.GetId());
				    LOG(INFO) << "# sg to id: " << sg_vertex_from.GetId() << std::endl;
                                    auto src_vertex_to_id = new_cand_map->at(sg_vertex_to.GetId());

                                    auto sg_key = std::make_pair(sg_vertex_from.GetId(), sg_vertex_to.GetId());
                                    auto src_key = std::make_pair(src_vertex_from_id, src_vertex_to_id);

                                    auto it = src_edge_labels.find(src_key);
                                    if (it == src_edge_labels.end() || it->second != sg_edge_labels[sg_key])
                                    {
                                        isomorphism = false;
                                    }
                                }
                            }

                            if (isomorphism)
                            {
                                
                                frequent_subgraphs.push_back(*new_cand_subgraph);
                            }
                        }
                        else
                        {
                            cand_subgraphs.push(std::make_pair(new_cand_subgraph, new_cand_map));
                        }
                    }
                }
            }
            return frequent_subgraphs;
        };
        auto update_results = [](DatasetPartition<Graph> &results, const DatasetPartition<Graph> &new_candidate) {
            // results.AppendPartition(new_candidate);
            for(const auto &candidate : new_candidate){
                results.push_back(candidate);
            }
        };

        // main loop
        auto candidates = std::make_shared<axe::common::Dataset<Graph>>(start_candidates);
        for (size_t i = 0; i < n_iters; i++)
        {
            candidates = std::make_shared<axe::common::Dataset<Graph>>(
                (*candidates).MapPartitionWith(shared_graphs.get(), extend_subgraph).MapPartitionWith(shared_graphs.get(), frequent_subgraph_isomorphism));
            results.UpdatePartitionWith(candidates.get(), update_results);
        }

        results.ApplyRead([](const DatasetPartition<Graph> &data) {
            for (auto &graph : data)
            {
                LOG(INFO) << "frequent subgraph: (" << graph.GetVertices() << ")@support=" << graph.GetSupport();
            }
            google::FlushLogFiles(google::INFO);
        });
        axe::common::JobDriver::ReversePrintTaskGraph(*tg);
    }
};

int main(int argc, char **argv)
{
    axe::common::JobDriver::Run(argc, argv, FSMNaive());
    return 0;
}
