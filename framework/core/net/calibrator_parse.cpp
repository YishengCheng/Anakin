#include "framework/core/net/calibrator_parse.h"

#ifndef USE_SGX
#include <fstream>
#include <iostream>
#include <sstream>
#endif

#include <unordered_set>

namespace anakin {

std::string layout2str(saber::LayoutType type) {
    switch (type) {
    case Layout_NCHW:
        return "nchw";

    case Layout_NHWC:
        return "nhwc";

    case Layout_NCHW_C8:
        return "nchw_c8";

    case Layout_NCHW_C8R:
        return "nchw_c8r";

    case Layout_NCHW_C4:
        return "nchw_c4";

    default:
        return "nchw";
    }
}
saber::LayoutType  str2layout(const std::string& str) {
    if (str == "nchw") {
        return Layout_NCHW;
    } else if (str == "nchw_c8") {
        return Layout_NCHW_C8;
    } else if (str == "nchw_c4") {
        return Layout_NCHW_C4;
    } else if (str == "nhwc") {
        return Layout_NHWC;
    } else if (str == "nchw_c8r") {
        return Layout_NCHW_C8R;
    } else {
        return Layout_NCHW;
    }
}



std::string CalibratorParser::get_precision(std::string name) const {
    //if not exist, return fp32
    if (_node_precision_map.find(name) == _node_precision_map.end()) {
        return "fp32";
    }

    return _node_precision_map.at(name);
}
saber::DataType CalibratorParser::get_dtype_of_precision(std::string name) const {
    std::string pre_str = "fp32";

    if (_node_precision_map.find(name) != _node_precision_map.end()) {
        pre_str = _node_precision_map.at(name);
    } else {

    }

    if (pre_str == "fp32") {
        return AK_FLOAT;
    } else if (pre_str == "int8") {
        return AK_INT8;
    } else if (pre_str == "uint8") {
        return AK_UINT8;
    } else {
        LOG(FATAL) << "unsupport precision type of " << pre_str;
    }

    return AK_FLOAT;
}

saber::DataType CalibratorParser::get_dtype(std::string name0, std::string name1,
        std::string bottom_op_type,
        std::string top_op_type, std::string dev_name, graph::NodePtr bottom_node) const {
    static std::unordered_set<std::string> layout_pass_op = {"Split", "Gather", "Pooling"};
    static std::unordered_set<std::string> conv_name_set = {"ConvBatchnormScaleRelu", "ConvRelu",
                                                            "ConvEltwise", "Convolution"};
    std::string str0 = get_precision(name0);
    std::string str1 = get_precision(name1);
    bool bint8 = ((str0 == "int8") && (str1 == "int8")) || ((str0 == "int8") && (str1 == "uint8"));
    //uint8 now use for x86, and x86 is 8bit perfer
    bool buint8 = str0 == "uint8" || (conv_name_set.count(bottom_op_type) && str1 == "uint8");
    LOG(INFO) << "get dtype string " << name0 << "," << str0 << "||" << name1 << "," << str1 << "||" <<
              bottom_op_type << "," << top_op_type;

    if (dev_name == "X86") {
#if defined(USE_X86_PLACE)
        bool top_8bit = (str1 == "int8" || str1 == "uint8");

        if (top_8bit && (conv_name_set.count(bottom_op_type) > 0)) {
            using pblock_type = PBlock<X86>;
            auto conv_weights = bottom_node->template get_attr<pblock_type>("weight_1");
            auto group = bottom_node->template get_attr<int>("group");
            bool is_inchannel_1_or_3 = conv_weights.shape().channel() == 1
                                       || conv_weights.shape().channel() == 3;

            if (is_inchannel_1_or_3 && group == 1) {
                bint8 = str1 == "int8";
                buint8 = str1 == "uint8";
            }
        }
#endif
    }

    if (bottom_op_type == "Input") {
        bint8 = (str0 == "int8");
        buint8 = (str0 == "uint8");
    }

    if (bint8) {
        return saber::AK_INT8;
    } else if (buint8) {
        return saber::AK_UINT8;
    } else {
        return saber::AK_FLOAT;
    }
}

saber::DataType CalibratorParser::get_dtype(std::string name0, std::string name1) const {
    std::string str0 = get_precision(name0);
    std::string str1 = get_precision(name1);
    bool bint8 = ((str0 == "int8") && (str1 == "int8")) || ((str0 == "int8") && (str1 == "uint8"));
    //uint8 now use for x86, and x86 is 8bit perfer
    bool buint8 = str0 == "uint8" ;


    if (bint8) {
        return saber::AK_INT8;
    } else if (buint8) {
        return saber::AK_UINT8;
    } else {
        return saber::AK_FLOAT;
    }
}

std::string CalibratorParser::get_target(std::string name) const {
    //if not exist, return NV
    if (_node_target_map.find(name) == _node_target_map.end()) {
#ifdef USE_CUDA
        return "NV";
#endif
#ifdef USE_X86_PLACE
        return "X86";
#endif
#ifdef USE_ARM_PLACE
        return "ARM";
#endif
    }

    return _node_target_map.at(name);
}
saber::LayoutType CalibratorParser::get_layout(const std::string name) const {
    //if not exist, return nchw
    if (_layout_map.find(name) == _layout_map.end()) {
        return Layout_NCHW;
    }

    return str2layout(_layout_map.at(name));
}

float CalibratorParser::get_calibrator(std::string name) const {
    //if not exist, return 1.0f
    if (_node_calibrator_map.find(name) == _node_calibrator_map.end()) {
        return 1.0f;
    }

    return _node_calibrator_map.at(name);
}
saber::LayoutType CalibratorParser::get_layout(std::string name0, std::string name1,
        saber::LayoutType old_layout) const {
    std::string str0 = get_precision(name0);
    std::string str1 = get_precision(name1);
    bool bint8 = (str0 == "int8") && (str1 == "int8");

    if (!bint8) {
        return old_layout;
    } else {
        return saber::Layout_NCHW_C4;
    }

}

saber::LayoutType CalibratorParser::get_layout(std::string name0, std::string name1,
        saber::LayoutType old_layout, std::string target_type,
        std::string bottom_op_name, std::string top_op_name, graph::NodePtr bottom_node) const {
    static std::unordered_set<std::string> conv_name_set = {"ConvBatchnormScaleRelu", "ConvRelu", "ConvEltwise", "Convolution"};

    if (target_type == "x86") {
#if defined(USE_X86_PLACE)
        std::string str0 = get_precision(name0);
        std::string str1 = get_precision(name1);
        bool bottom_8bit = (str0 == "int8" || str0 == "uint8");
        bool top_8bit = (str1 == "int8" || str1 == "uint8");
        bool bint8 = bottom_8bit && top_8bit;

        if (top_8bit && (conv_name_set.count(bottom_op_name) > 0)) {

            using pblock_type = PBlock<X86>;
            auto conv_weights = bottom_node->template get_attr<pblock_type>("weight_1");
            auto group = bottom_node->template get_attr<int>("group");
            bool is_inchannel_1_or_3 = conv_weights.shape().channel() == 1
                                       || conv_weights.shape().channel() == 3;

            if (is_inchannel_1_or_3 && group == 1) {
                bint8 = true;
            }

        }

        if (bottom_8bit && bottom_op_name == "Pooling"){
            bint8 = true;
        }

        LOG(INFO) << "get get_layout " << str0 << "," << str1 << " old layout " << old_layout << ",bint8 "
                  << bint8;

        if (!bint8) {
            return old_layout;
        } else {
            return saber::Layout_NHWC;
        }
#endif
    } else {
        LOG(FATAL) << "not support target type " << target_type;
    }

    return old_layout;

}

void CalibratorParser::set_precision(std::string name, saber::DataType type) {
    std::string str = "fp32";

    switch (type) {
    case AK_FLOAT:
        str = "fp32";
        break;

    case AK_INT8:
        str = "int8";
        break;

    case AK_UINT8:
        str = "uint8";
        break;

    default:
        break;
    }

    _node_precision_map[name] = str;
}
void CalibratorParser::set_precision(std::string name, std::string type) {
    _node_precision_map[name] = type;
}
void CalibratorParser::set_scale(std::string name, float scale) {
    _node_calibrator_map[name] = scale;
}
void CalibratorParser::set_layout(std::string name, saber::LayoutType layout) {
    _layout_map[name] = layout2str(layout);
}
void CalibratorParser::set_layout(std::string name, std::string layout_name) {
    _layout_map[name] = layout_name;
}
#ifndef USE_SGX
void CalibratorParser::auto_config(const std::vector<std::string>& exe_nodes,
                                   const std::vector<std::string>& op_names, std::string dst,
                                   std::string precision, std::string target) {
    std::fstream fs;
    fs.open(dst, std::ios::in);

    if (fs) {
        fs.close();
        LOG(WARNING) << "config file already existed, will not be created ";
        return;
    }

    LOG(WARNING) << "config file not existed, creating it ";
    std::ofstream ofs(dst);

    if (!ofs.is_open()) {
        LOG(FATAL) << "open file " << dst << "failed";
    }

    for (int i = 0; i < exe_nodes.size(); ++i) {
        std::string name = exe_nodes[i];

        if (!name.empty()) {
            std::string op_name = op_names[i];
            ofs << name << "(" << op_name << ")    " << precision << "    " << target << " \n";
        }
    }

    ofs.close();
}
void CalibratorParser::auto_config_layout(const std::vector<std::string>& names,
        const std::vector<saber::LayoutType >& layouts, std::string dst) {
    std::fstream fs;
    fs.open(dst, std::ios::in);

    if (fs) {
        fs.close();
        LOG(WARNING) << "config file already existed, will not be created ";
        return;
    }

    LOG(WARNING) << "config file not existed, creating it ";
    std::ofstream ofs(dst);

    if (!ofs.is_open()) {
        LOG(FATAL) << "open file " << dst << "failed";
    }

    for (int i = 0; i < names.size(); ++i) {
        std::string name = names[i];

        if (!name.empty()) {
            std::string layout = layout2str(layouts[i]);
            ofs << name << " " << layout << " \n";
        }
    }

    ofs.close();
}

void CalibratorParser::parse_from_file(std::string config, std::string calibrator) {
    _config_parse(config);
    _calibrator_parse(calibrator);
}

void CalibratorParser::_config_parse(std::string config) {
    std::ifstream ifs(config);

    if (!ifs.is_open()) {
        LOG(ERROR) << "open file " << config << " failed, will use default config";
        return;
    }

    std::string line;

    while (ifs.good()) {
        std::getline(ifs, line);

        if (!line.empty()) {
            auto str_vec = _line_config_parse(line);
            std::string node_name;

            //            LOG(INFO)<<"read config "<<str_vec[0];
            if (str_vec.size() >= 1) {
                node_name = str_vec[0];
                node_name.erase(node_name.find_last_of("("));
            }

            if (str_vec.size() >= 3) {
                _node_target_map[node_name] = str_vec[2];
            }

            if (str_vec.size() >= 2) {
                _node_precision_map[node_name] = str_vec[1];
                //                LOG(INFO)<<"parser _node_precision_map "<<_node_precision_map[node_name];
            }

            LOG(INFO) << "parse " << line << ", get " << node_name << ",size = " << str_vec.size();
        }
    }

    ifs.close();
}

void CalibratorParser::_calibrator_parse(std::string calibrator) {
    std::ifstream ifs(calibrator);

    if (!ifs.is_open()) {
        LOG(WARNING) << "open file " << calibrator << "failed!, will use default calibrator";
        return;
    }

    std::string line;

    while (ifs.good()) {
        std::getline(ifs, line);

        if (!line.empty()) {
            _line_calibrator_parse(line);
        }
    }

    ifs.close();
}
#ifdef BUILD_LITE
std::string convert2underline(std::string& name) {
    char* target = strdup(name.c_str());

    for (char* p = target; *p != '\0'; ++p) {
        if (*p == '-') {
            *p = '_';
        } else if (*p == '/') {
            *p = '_';
        }
    }

    std::string str_tmp = target;
    free(target);
    return str_tmp;
};
#endif
std::vector<std::string> CalibratorParser::_line_config_parse(std::string line) {
    line.erase(line.find_last_not_of("\n") + 1);
    line.erase(line.find_last_not_of(" ") + 1);
    std::istringstream iss(line);
    std::string temp;
    std::vector<std::string> str_vec;

    while (iss.good()) {
        iss >> temp;
        str_vec.push_back(temp);
    }

#ifdef BUILD_LITE
    str_vec[0] = convert2underline(str_vec[0]);
#endif
    return str_vec;
}

void CalibratorParser::_line_calibrator_parse(std::string line) {
    line.erase(line.find_last_not_of("\n") + 1);
    line.erase(line.find_last_not_of(" ") + 1);
    std::istringstream iss(line);
    std::string name;
    float value = 1.0f;

    if (iss.good()) {
        iss >> name;
    }

    try {
        if (iss.good()) {
            iss.precision(7);
            iss >> value;
        }
    } catch (std::exception& e) {
        LOG(FATAL) << "calibrator load wrong!! line:" << line;
    }

#ifdef BUILD_LITE
    name = convert2underline(name);
#endif
    _node_calibrator_map[name] = value;
}

void  CalibratorParser::layout_parse(std::string layout) {
    std::ifstream ifs(layout);

    if (!ifs.is_open()) {
        LOG(WARNING) << "open file " << layout << " failed!, will use default calibrator";
        return;
    } else {
        LOG(INFO) << "open file layout config success " << layout;
    }

    std::string line;

    while (ifs.good()) {
        std::getline(ifs, line);

        if (!line.empty()) {
            _line_layout_parse(line);
        }
    }

    ifs.close();
}
void CalibratorParser::_line_layout_parse(std::string line) {
    line.erase(line.find_last_not_of("\n") + 1);
    line.erase(line.find_last_not_of(" ") + 1);
    std::istringstream iss(line);
    std::string temp;
    std::vector<std::string> str_vec;

    while (iss.good()) {
        iss >> temp;
        str_vec.push_back(temp);
    }

    if (str_vec.size() >= 2) {
        _layout_map[str_vec[0]] = str_vec[1];
    }
}
#endif // USE_SGX

void CalibratorParser::clear_data() {
    _node_precision_map.clear();
    _node_calibrator_map.clear();
    _node_target_map.clear();
    _layout_map.clear();
}


}

