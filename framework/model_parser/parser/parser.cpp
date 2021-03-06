#include "framework/model_parser/parser/parser.h"
#include "framework/model_parser/parser/model_io.h"
#ifdef USE_NANOPB
#include "graph.pb.hpp"
#include "node.pb.hpp"
#include "operator.pb.hpp"
#include "tensor.pb.hpp"
#else
#include <google/protobuf/io/zero_copy_stream_impl.h>
#include <google/protobuf/io/zero_copy_stream.h>
#include <google/protobuf/text_format.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <fstream>
#include <sys/types.h>

#include "graph.pb.h"
#include "node.pb.h"
#include "operator.pb.h"
#include "tensor.pb.h"
#endif

namespace anakin {
namespace parser {

const char * WaterMark = "Anakin@right";

template<typename Ttype, Precision Ptype>
Status load(graph::Graph<Ttype, Ptype>* graph, std::string& model_path) {
    return load(graph, model_path.c_str());
}

Status parse_graph_proto(GraphProto& graph_proto, const char* buffer, size_t len) {
#ifdef USE_NANOPB
    bool success = graph_proto.parse_from_buffer(buffer, len);
#else
    google::protobuf::io::ArrayInputStream raw_input(buffer, len);
    google::protobuf::io::CodedInputStream coded_input(&raw_input);
    coded_input.SetTotalBytesLimit(INT_MAX, 536870912);
    bool success = graph_proto.ParseFromCodedStream(&coded_input) && coded_input.ConsumedEntireMessage();
#endif
    if (!success) {
        LOG(ERROR) << " Parsing GraphProto " << " ERROR";
        return Status::ANAKINFAIL("Parsing GraphProto ERROR");
    }
    return Status::OK();
}

Status parse_graph_proto(GraphProto& graph_proto, const char* model_path) {
#ifdef USE_NANOPB
    FILE *f = fopen(model_path, "rb");
    graph_proto.parse_from_file(f);
    fclose(f);
    return Status::OK();
#else
    int file_descriptor = open(model_path, O_RDONLY);

    if (file_descriptor == -1) {
        LOG(FATAL) << " Can't open " << model_path;
    }

    google::protobuf::io::FileInputStream raw_input(file_descriptor);

    google::protobuf::io::CodedInputStream coded_input(&raw_input);

    coded_input.SetTotalBytesLimit(ProtoReadBytesLimit, 536870912);

    bool success = graph_proto.ParseFromCodedStream(&coded_input);

    if (!success) {
        LOG(ERROR) << " Parsing GraphProto " << model_path << " ERROR";
        return Status::ANAKINFAIL("Parsing GraphProto ERROR");
    }

    close(file_descriptor);
    return Status::OK();
#endif
}

bool InspectAnakin(const std::string& model_path) {
    GraphProto graph_proto;
    auto ret = parse_graph_proto(graph_proto, model_path.c_str());
    if(ret) {
        return true;
    }
    return false;
}

bool InspectAnakin(const char* buffer, size_t len) {
    GraphProto graph_proto;
    auto ret = parse_graph_proto(graph_proto, buffer, len);
    if(ret) {
        return true;
    }
    return false;
}

template<typename Ttype, Precision Ptype>
Status generate_graph_with_graph_proto(graph::Graph<Ttype, Ptype>* graph, GraphProto& graph_proto) {
    // fill the graph with name
    LOG(INFO) << "graph name: " << graph_proto.name();
    graph->set_name(graph_proto.name());

    // fill the graph with ins/outs
    for (int i = 0; i < graph_proto.ins().size(); i++) {
        LOG(INFO) << "graph in: " << graph_proto.ins()[i];
        std::string in_name(graph_proto.ins()[i]);
        graph->add_in(in_name);
    }

    for (int i = 0; i < graph_proto.outs().size(); i++) {
        LOG(INFO) << "graph out: " << graph_proto.outs()[i];
        std::string out_name(graph_proto.outs()[i]);
        graph->add_out(out_name);
    }

    // fill the graph with nodes
    NodeIO<Ttype, Ptype> node_io;

    for (int i = 0; i < graph_proto.nodes().size(); i++) {
        node_io >> graph_proto.nodes()[i];
    }

    node_io << *graph;

    // fill the graph with edges
    auto it_in = graph_proto.edges_in().begin();

    for (; it_in != graph_proto.edges_in().end(); ++it_in) {
        auto& key = it_in->first;
        auto& second = it_in->second;
        if (second.target().size() > 0) {
            for (int i = 0; i < second.target().size(); i++) {
                DLOG(INFO) << "Parsing in edges of node with scale: " << key;
                graph::Edge<Ttype> edge(second.target()[i].node(), key);
                std::vector<float> scale;
                for (int j = 0; j < second.target()[i].scale_size(); j++) {
                    scale.push_back(second.target()[i].scale(j));
                }
                auto layout = second.target()[i].layout();
                if (layout == 0){
                    layout = LP_NCHW;
                }
                edge.set_scale(scale);
                edge.set_layout((anakin::saber::LayoutType)layout);
                edge.shared() = (*graph_proto.mutable_edges_info())[edge.name()].shared();
                edge.share_from() = (*graph_proto.mutable_edges_info())[edge.name()].share_from();
                graph->add_in_arc(edge);
            }
        } else {
            for (int i = 0; i < second.val().size(); i++) {
                DLOG(INFO) << "Parsing in edges of node without scale: " << key;
                graph::Edge<Ttype> edge(second.val()[i], key);
                edge.shared() = (*graph_proto.mutable_edges_info())[edge.name()].shared();
                edge.share_from() = (*graph_proto.mutable_edges_info())[edge.name()].share_from();
                graph->add_in_arc(edge);
            }
        }
    }

    auto it_out = graph_proto.edges_out().begin();

    for (; it_out != graph_proto.edges_out().end(); ++it_out) {
        auto& key = it_out->first;
        auto& second = it_out->second;
        if (second.target().size() > 0) {
            for (int i = 0; i < second.target().size(); i++) {
                DLOG(INFO) << "Parsing out edges of node with scale: " << key;
                graph::Edge<Ttype> edge(key, second.target()[i].node());
                std::vector<float> scale;
                for (int j = 0; j < second.target()[i].scale_size(); j++) {
                    scale.push_back(second.target()[i].scale(j));
                }
                auto layout = second.target()[i].layout();
                DLOG(ERROR) << "layout:" << layout;
                if (layout == 0){
                    layout = LP_NCHW;
                }
                edge.set_scale(scale);
                edge.set_layout((anakin::saber::LayoutType)layout);
                edge.shared() = (*graph_proto.mutable_edges_info())[edge.name()].shared();
                edge.share_from() = (*graph_proto.mutable_edges_info())[edge.name()].share_from();
                graph->add_out_arc(edge);
            }
        } else {
            for (int i = 0; i < second.val().size(); i++) {
                DLOG(INFO) << "Parsing in edges of node without scale: " << key;
                graph::Edge<Ttype> edge(key, second.val()[i]);
                edge.shared() = (*graph_proto.mutable_edges_info())[edge.name()].shared();
                edge.share_from() = (*graph_proto.mutable_edges_info())[edge.name()].share_from();
                graph->add_out_arc(edge);
            }
        }
    }

    // fill the graph with info (only use the key value: is_optimized)
    graph->statistics.template set_info<graph::IS_OPTIMIZED>(graph_proto.summary().is_optimized());
    graph->statistics.template set_info<graph::TEMP_MEM>(graph_proto.summary().temp_mem_used());
    graph->statistics.template set_info<graph::ORI_TEMP_MEM>
    (graph_proto.summary().original_temp_mem_used());
    graph->statistics.template set_info<graph::SYSTEM_MEM>(graph_proto.summary().system_mem_used());
    graph->statistics.template set_info<graph::MODEL_MEM>(graph_proto.summary().model_mem_used());
    return Status::OK();
}

template<typename Ttype, Precision Ptype>
Status load(graph::Graph<Ttype, Ptype>* graph, const char* model_path) {
    GraphProto graph_proto;
    parse_graph_proto(graph_proto, model_path);
    return generate_graph_with_graph_proto(graph, graph_proto);
}

template<typename Ttype, Precision Ptype>
Status load(graph::Graph<Ttype, Ptype>* graph, const char* buffer, size_t len) {
    GraphProto graph_proto;
    parse_graph_proto(graph_proto, buffer, len);
    return generate_graph_with_graph_proto(graph, graph_proto);
}

#ifndef USE_NANOPB
template<typename Ttype, Precision Ptype>
Status save(graph::Graph<Ttype, Ptype>* graph, std::string& model_path) {
    return save(graph, model_path.c_str());
}

template<typename Ttype, Precision Ptype>
Status save(graph::Graph<Ttype, Ptype>* graph, const char* model_path) {
    std::fstream output(model_path, std::ios::out | std::ios::trunc | std::ios::binary);

    if (!output) {
        LOG(ERROR) << model_path << " : File not found. ";
        return Status::ANAKINFAIL("File not found");
    }

    GraphProto graph_proto;
    // TODO...  fill the graph_proto with graph.
    // set graph proto name
    graph_proto.set_name(graph->name());

    // fill the graph proto with ins/outs
    for (auto in : graph->get_ins()) {
        graph_proto.add_ins(in);
    }

    for (auto out : graph->get_outs()) {
        graph_proto.add_outs(out);
    }

    // fill the graph proto  nodes with NodePtr in exec order
    NodeIO<Ttype, Ptype> node_io;
    auto nodes_in_exec_order = graph->get_nodes_in_order();

    // accept node without shared weights
    for(int i = 0; i < nodes_in_exec_order.size(); i++) {
        if(!((*graph)[nodes_in_exec_order[i]]->is_weight_shared())) {
            node_io >> (*graph)[nodes_in_exec_order[i]];
        }
    }

    // accept node with shared weights
    for (int i = 0; i < nodes_in_exec_order.size(); i++) {
        if((*graph)[nodes_in_exec_order[i]]->is_weight_shared()) {
            node_io >> (*graph)[nodes_in_exec_order[i]];
        }
    }

    node_io << graph_proto;

    // fill the graph proto' edges/edges_info with edges
    auto edges_in = graph_proto.mutable_edges_in();
    auto edges_out = graph_proto.mutable_edges_out();
    auto edges_info = graph_proto.mutable_edges_info();
    auto insert_edge = [&](graph::NodePtr& node_p) {
        auto& arcs_it_in = graph->get_in_arc_its(node_p->name());
        auto& arcs_it_out = graph->get_out_arc_its(node_p->name());
        for (auto& edge_it : arcs_it_in) {
            auto tg = (*edges_in)[edge_it->second()].add_target();
            tg->set_node(edge_it->first());
            for (auto scale: edge_it->scale()){
                tg->add_scale(scale);
            }
            tg->set_layout((LayoutProto)edge_it->layout());
            TensorProto ts;
            ts.set_name(edge_it->name());
            ts.set_shared(edge_it->shared());
            ts.set_share_from(edge_it->share_from());
            (*edges_info)[edge_it->name()].CopyFrom(ts);
        }

        for (auto& edge_it : arcs_it_out) {
            auto tg = (*edges_out)[edge_it->first()].add_target();
            tg->set_node(edge_it->second());
            for (auto scale: edge_it->scale()){
                tg->add_scale(scale);
            }
            tg->set_layout((LayoutProto)edge_it->layout());
            TensorProto ts;
            ts.set_name(edge_it->name());
            ts.set_shared(edge_it->shared());
            ts.set_share_from(edge_it->share_from());
            (*edges_info)[edge_it->name()].CopyFrom(ts);
        }
    };

    graph->Scanner->BFS(insert_edge);


    // save graph info
    auto summary = graph_proto.mutable_summary();
    summary->set_is_optimized(graph->statistics.template get_info<graph::IS_OPTIMIZED>());
    summary->set_temp_mem_used(graph->statistics.template get_info<graph::TEMP_MEM>());
    summary->set_original_temp_mem_used(graph->statistics.template get_info<graph::ORI_TEMP_MEM>());
    summary->set_system_mem_used(graph->statistics.template get_info<graph::SYSTEM_MEM>());
    summary->set_model_mem_used(graph->statistics.template get_info<graph::MODEL_MEM>());

    //  save graph proto to disk
    graph_proto.SerializeToOstream(&output);

    return Status::OK();
}
#endif

#ifdef USE_CUDA
template
Status load<NV, Precision::FP32>(graph::Graph<NV, Precision::FP32>* graph, const char* model_path);
template
Status load<NV, Precision::FP16>(graph::Graph<NV, Precision::FP16>* graph, const char* model_path);
template
Status load<NV, Precision::INT8>(graph::Graph<NV, Precision::INT8>* graph, const char* model_path);
template
Status save<NV, Precision::FP32>(graph::Graph<NV, Precision::FP32>* graph, std::string& model_path);
template
Status save<NV, Precision::FP16>(graph::Graph<NV, Precision::FP16>* graph, std::string& model_path);
template
Status save<NV, Precision::INT8>(graph::Graph<NV, Precision::INT8>* graph, std::string& model_path);

template
Status load<NV, Precision::FP32>(graph::Graph<NV, Precision::FP32>* graph, std::string& model_path);
template
Status load<NV, Precision::FP16>(graph::Graph<NV, Precision::FP16>* graph, std::string& model_path);
template
Status load<NV, Precision::INT8>(graph::Graph<NV, Precision::INT8>* graph, std::string& model_path);

template
Status save<NV, Precision::FP32>(graph::Graph<NV, Precision::FP32>* graph, const char* model_path);
template
Status save<NV, Precision::FP16>(graph::Graph<NV, Precision::FP16>* graph, const char* model_path);
template
Status save<NV, Precision::INT8>(graph::Graph<NV, Precision::INT8>* graph, const char* model_path);

template
Status load<NV, Precision::FP32>(graph::Graph<NV, Precision::FP32>* graph, const char* buffer, size_t len);
template
Status load<NV, Precision::FP16>(graph::Graph<NV, Precision::FP16>* graph, const char* buffer, size_t len);
template
Status load<NV, Precision::INT8>(graph::Graph<NV, Precision::INT8>* graph, const char* buffer, size_t len);
#endif

#if defined USE_X86_PLACE || defined BUILD_LITE
template
Status load<X86, Precision::FP32>(graph::Graph<X86, Precision::FP32>* graph, const char* model_path);
template
Status load<X86, Precision::FP16>(graph::Graph<X86, Precision::FP16>* graph, const char* model_path);
template
Status load<X86, Precision::INT8>(graph::Graph<X86, Precision::INT8>* graph, const char* model_path);

#ifndef USE_NANOPB
template
Status save<X86, Precision::FP32>(graph::Graph<X86, Precision::FP32>* graph, std::string& model_path);
template
Status save<X86, Precision::FP16>(graph::Graph<X86, Precision::FP16>* graph, std::string& model_path);
template
Status save<X86, Precision::INT8>(graph::Graph<X86, Precision::INT8>* graph, std::string& model_path);
#endif

template
Status load<X86, Precision::FP32>(graph::Graph<X86, Precision::FP32>* graph, std::string& model_path);
template
Status load<X86, Precision::FP16>(graph::Graph<X86, Precision::FP16>* graph, std::string& model_path);
template
Status load<X86, Precision::INT8>(graph::Graph<X86, Precision::INT8>* graph, std::string& model_path);

#ifndef USE_NANOPB
template
Status save<X86, Precision::FP32>(graph::Graph<X86, Precision::FP32>* graph, const char* model_path);
template
Status save<X86, Precision::FP16>(graph::Graph<X86, Precision::FP16>* graph, const char* model_path);
template
Status save<X86, Precision::INT8>(graph::Graph<X86, Precision::INT8>* graph, const char* model_path);
#endif

template
Status load<X86, Precision::FP32>(graph::Graph<X86, Precision::FP32>* graph, const char* buffer, size_t len);
template
Status load<X86, Precision::FP16>(graph::Graph<X86, Precision::FP16>* graph, const char* buffer, size_t len);
template
Status load<X86, Precision::INT8>(graph::Graph<X86, Precision::INT8>* graph, const char* buffer, size_t len);
#endif

#ifdef USE_ARM_PLACE
template
Status load<ARM, Precision::FP32>(graph::Graph<ARM, Precision::FP32>* graph, const char* model_path);
template
Status load<ARM, Precision::FP32>(graph::Graph<ARM, Precision::FP32>* graph, std::string& model_path);
template
Status load<ARM, Precision::FP32>(graph::Graph<ARM, Precision::FP32>* graph, const char* buffer, size_t len);
#ifndef USE_NANOPB
template
Status save<ARM, Precision::FP32>(graph::Graph<ARM, Precision::FP32>* graph, std::string& model_path);
template
Status save<ARM, Precision::FP32>(graph::Graph<ARM, Precision::FP32>* graph, const char* model_path);
#endif

template
Status load<ARM, Precision::FP16>(graph::Graph<ARM, Precision::FP16>* graph, const char* model_path);
template
Status load<ARM, Precision::FP16>(graph::Graph<ARM, Precision::FP16>* graph, std::string& model_path);
template
Status load<ARM, Precision::FP16>(graph::Graph<ARM, Precision::FP16>* graph, const char* buffer, size_t len);

#ifndef USES_NANOPB
template
Status save<ARM, Precision::FP16>(graph::Graph<ARM, Precision::FP16>* graph, std::string& model_path);
template
Status save<ARM, Precision::FP16>(graph::Graph<ARM, Precision::FP16>* graph, const char* model_path);
#endif

template
Status load<ARM, Precision::INT8>(graph::Graph<ARM, Precision::INT8>* graph, const char* model_path);
template
Status load<ARM, Precision::INT8>(graph::Graph<ARM, Precision::INT8>* graph, std::string& model_path);
template
Status load<ARM, Precision::INT8>(graph::Graph<ARM, Precision::INT8>* graph, const char* buffer, size_t len);

#ifndef USE_NANOPB
template
Status save<ARM, Precision::INT8>(graph::Graph<ARM, Precision::INT8>* graph, const char* model_path);
template
Status save<ARM, Precision::INT8>(graph::Graph<ARM, Precision::INT8>* graph, std::string& model_path);
#endif
#endif // ifdef USE_ARM_PLACE


#ifdef AMD_GPU
template
Status load<AMD, Precision::FP32>(graph::Graph<AMD, Precision::FP32>* graph, std::string& model_path);
template
Status load<AMD, Precision::FP16>(graph::Graph<AMD, Precision::FP16>* graph, std::string& model_path);
template
Status load<AMD, Precision::INT8>(graph::Graph<AMD, Precision::INT8>* graph, std::string& model_path);

template
Status load<AMD, Precision::FP32>(graph::Graph<AMD, Precision::FP32>* graph, const char* model_path);
template
Status load<AMD, Precision::FP16>(graph::Graph<AMD, Precision::FP16>* graph, const char* model_path);
template
Status load<AMD, Precision::INT8>(graph::Graph<AMD, Precision::INT8>* graph, const char* model_path);

template
Status load<AMD, Precision::FP32>(graph::Graph<AMD, Precision::FP32>* graph, const char* buffer, size_t len);
template
Status load<AMD, Precision::FP16>(graph::Graph<AMD, Precision::FP16>* graph, const char* buffer, size_t len);
template
Status load<AMD, Precision::INT8>(graph::Graph<AMD, Precision::INT8>* graph, const char* buffer, size_t len);

#ifndef USE_NANOPB
template
Status save<AMD, Precision::FP32>(graph::Graph<AMD, Precision::FP32>* graph, std::string& model_path);
template
Status save<AMD, Precision::FP16>(graph::Graph<AMD, Precision::FP16>* graph, std::string& model_path);
template
Status save<AMD, Precision::INT8>(graph::Graph<AMD, Precision::INT8>* graph, std::string& model_path);

template
Status save<AMD, Precision::FP32>(graph::Graph<AMD, Precision::FP32>* graph, const char* model_path);
template
Status save<AMD, Precision::FP16>(graph::Graph<AMD, Precision::FP16>* graph, const char* model_path);
template
Status save<AMD, Precision::INT8>(graph::Graph<AMD, Precision::INT8>* graph, const char* model_path);
#endif
#endif

} /* parser */

} /* anakin */
