#include <iostream>

#include <string>
#include <sstream>
#include <filesystem>
#include <vector>
#include <regex>
#include <fstream>
#include <optional>
#include <filesystem>

#include <arrow/api.h>
#include <arrow/io/api.h>
#include <parquet/arrow/reader.h>
#include <parquet/arrow/writer.h>
#include <parquet/stream_reader.h>
#include <parquet/stream_writer.h>

using parquet::ArrowWriterProperties;
using parquet::WriterProperties;

enum class TraceOperation {
    READ = 0,
    WRITE = 1,
    CLFLUSH = 2
};


struct TraceEntry {
public:
    TraceEntry(const TraceOperation op, const size_t op_size, const unsigned int opcode, const double timestamp_sec, const unsigned long abs_addr, const unsigned long rel_addr, const unsigned long long data) :
        op(op),
        op_size(op_size),
        opcode(opcode),
        timestamp_sec(timestamp_sec),
        abs_addr(abs_addr),
        rel_addr(rel_addr),
        dax_addr(nullptr),
        data(data)
    {};

    const TraceOperation op;
    const size_t op_size;
    const unsigned int opcode;
    const double timestamp_sec;
    const unsigned long abs_addr;
    const unsigned long rel_addr;
    void* dax_addr;
    //const std::vector<uint8_t> data;
    const unsigned long data;
};


arrow::Status trace_to_arrow(std::filesystem::path path, const std::vector<TraceEntry>& trace_entries)
{
    std::shared_ptr<WriterProperties> props =
        WriterProperties::Builder().compression(arrow::Compression::GZIP)->build();

    std::shared_ptr<arrow::io::FileOutputStream> outfile;

    path = path.replace_extension(".parquet");

    PARQUET_ASSIGN_OR_THROW(
        outfile,
        arrow::io::FileOutputStream::Open(path));

    parquet::WriterProperties::Builder builder;
    std::shared_ptr<parquet::schema::GroupNode> schema;
    
    
    schema = std::static_pointer_cast<parquet::schema::GroupNode>(parquet::schema::GroupNode::Make(
        "MyData", parquet::Repetition::REQUIRED, {
            parquet::schema::PrimitiveNode::Make("timestamp", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
            parquet::schema::PrimitiveNode::Make("op", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("opcode", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("op_size", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("abs_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("rel_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("data", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
        }
    ));
    
    parquet::StreamWriter os{
        parquet::ParquetFileWriter::Open(outfile, schema, props)};

    for (const auto& entry : trace_entries) {
        os << entry.timestamp_sec << static_cast<uint32_t>(entry.op) << entry.opcode << entry.op_size << entry.abs_addr << entry.rel_addr << entry.data << parquet::EndRow;
    }
    

    return arrow::Status::OK();
}

template<size_t N>
bool tokenize(std::string const &str, std::array<std::string, N>& out)
{
	std::stringstream ss(str);

	std::string s;
	size_t i = 0;

	while (std::getline(ss, s, ' ')) {
		if ((i + 1) > N) {
			return false;
		}

		out[(i++)] = s;
	}

	return (i == N);
}


int compress_trace(std::filesystem::path path)
{
    std::ifstream trace_handle(path);

    if (!trace_handle) {
        std::cerr << "Failed to open trace file: " << path << std::endl;
        return -1;
    }

    std::string line;

    if (!std::getline(trace_handle, line)) {
        return -1;
    }

    std::regex re(R"(PMEMTRACE DEVICE:\s+\[(0x[\da-fA-F]+)-(0x[\da-fA-F]+)\])");
    std::smatch match;

    if (!std::regex_search(line, match, re)) {
        std::cerr << "Invalid trace file format" << std::endl;
        return -1;
    }

    const auto pmem_range_start = std::stoul(match[1].str(), nullptr, 16);
    const auto pmem_range_end = std::stoul(match[2].str(), nullptr, 16);
    std::cout << "Start address: " << std::hex << pmem_range_start << '\n';
    std::cout << "End address: " << std::hex << pmem_range_end << '\n';
    std::cout << "Size: " << std::dec << (pmem_range_end - pmem_range_start) / (1024 * 1024 * 1024) << " GiB" << std::endl; 
   
    std::regex pattern(R"((R|W|F)\s+(\d+)\s+(0x[\da-fA-F]+)\s+([\d.]+)\s+\d+\s+(0x[\da-fA-F]+)\s+(0x[\da-fA-F]+))");
    //TraceFile trace;
    std::smatch matches;
    

    std::shared_ptr<WriterProperties> props =
        WriterProperties::Builder().compression(arrow::Compression::GZIP)->build();

    std::shared_ptr<arrow::io::FileOutputStream> outfile;

    path = path.replace_extension(".parquet");

    PARQUET_ASSIGN_OR_THROW(
        outfile,
        arrow::io::FileOutputStream::Open(path));

    parquet::WriterProperties::Builder builder;
    std::shared_ptr<parquet::schema::GroupNode> schema;
    
    
    // https://arrow.apache.org/docs/cpp/parquet.html
    schema = std::static_pointer_cast<parquet::schema::GroupNode>(parquet::schema::GroupNode::Make(
        path.filename(), parquet::Repetition::REQUIRED, {
            parquet::schema::PrimitiveNode::Make("timestamp", parquet::Repetition::REQUIRED, parquet::Type::DOUBLE),
            parquet::schema::PrimitiveNode::Make("op", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("opcode", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
            parquet::schema::PrimitiveNode::Make("op_size", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("abs_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("rel_addr", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
            parquet::schema::PrimitiveNode::Make("data", parquet::Repetition::REQUIRED, parquet::Type::INT64, parquet::ConvertedType::UINT_64),
			parquet::schema::PrimitiveNode::Make("cpu_id", parquet::Repetition::REQUIRED, parquet::Type::INT32, parquet::ConvertedType::UINT_32),
        }
    ));
    
    TraceOperation op;
    parquet::StreamWriter os{
        parquet::ParquetFileWriter::Open(outfile, schema, props)};

    std::array<std::string, 9> tokens_line;

    while (std::getline(trace_handle, line)) {
		if (!tokenize(line, tokens_line)) {
			continue;
		}

		if (tokens_line[0] == "R") {
			op = TraceOperation::READ;
		} else if (tokens_line[0] == "W") {
			op = TraceOperation::WRITE;
		} else if (tokens_line[0] == "F") {
			op = TraceOperation::CLFLUSH;
		} else {
			//std::cerr << "Unknown trace operation: " << tokens_line[0] << std::endl;

			continue;
			//return false;
		}

		const double timestamp_sec = std::stod(tokens_line[3]);
		const unsigned int opcode = std::stoi(tokens_line[2], nullptr, 16);
		const size_t opcode_size = std::stoul(tokens_line[1]);
		const unsigned long abs_addr = std::stoul(tokens_line[5], nullptr, 16);
		#ifdef ENABLE_ASSERTS
		assert(abs_addr > pmem_range_strawnameart);
		assert(abs_addr < pmem_range_end);
		#endif
		const unsigned long rel_addr = abs_addr - pmem_range_start;
		const unsigned long long data = std::stoull(tokens_line[6], nullptr, 16);
		const unsigned int cpu_id = std::stoi(tokens_line[8]);

		//std::cout << timestamp_sec << " " << static_cast<uint32_t>(op) << " " << std::hex << opcode << " " << std::dec << opcode_size << " 0x" << std::hex << abs_addr << " 0x" << rel_addr << " 0x" << data << " " << std::dec << cpu_id << std::endl;


		os << timestamp_sec << static_cast<uint32_t>(op) << opcode << opcode_size << abs_addr << rel_addr << static_cast<uint64_t>(data) << cpu_id << parquet::EndRow;
    }


    std::cout << "num columns: " << std::dec << os.num_columns() << std::endl;

    // std::cout << std::dec << trace_entries.size() << " entries" << std::endl;

    // arrow::Status st = trace_to_arrow(std::filesystem::path(filename), trace_entries);

    // if (!st.ok()) {
    //     std::cerr << st << std::endl;
    //     return -1;
    // }
   
    return 0;
}

void read_parquet_trace(std::filesystem::path& path)
{
    // https://arrow.apache.org/docs/cpp/parquet.html
    std::shared_ptr<arrow::io::ReadableFile> infile;

    PARQUET_ASSIGN_OR_THROW(
      infile,
      arrow::io::ReadableFile::Open(path));

    parquet::StreamReader stream {parquet::ParquetFileReader::Open(infile)};

    double timestamp_sec;
    uint32_t op;
    TraceOperation trace_op;
    unsigned int opcode;
    size_t opcode_size;
    unsigned long abs_addr;
    unsigned long rel_addr;
    unsigned long data;
    unsigned int cpu_id;

    size_t count_reads = 0, count_writes = 0, count_flushes = 0;

    while (!(stream.eof())) {
        stream >> timestamp_sec >> op >> opcode >> opcode_size >> abs_addr >> rel_addr >> data >> cpu_id >> parquet::EndRow;
        //std::cout << "OP: " << op << " opcode: 0x" << std::hex << opcode << " timestamp: " << timestamp_sec << " 0x" << std::hex << abs_addr << " data: 0x" << data << std::endl;

        trace_op = static_cast<TraceOperation>(op);


        switch (trace_op) {
            case TraceOperation::READ:
                ++count_reads; break;
            case TraceOperation::WRITE:
                ++count_writes; break;
            case TraceOperation::CLFLUSH:
                ++count_flushes; break;
            default:
                break;
        }
    }

    std::cout << "Reads: " << std::dec << count_reads << " Writes: " << count_writes << " Flushes: " << count_flushes << std::endl;
}

int main(int argc, char* argv[]) {
    //std::cout << "Hello World!\n" << std::endl;

    if (argc < 2)
        return -1;

    const int ret_code = compress_trace(std::string(argv[1]));
    // std::cout << ret_code << std::endl;

    //read_parquet_trace(std::filesystem::path(argv[1]).replace_extension(".parquet"));

    return 0;
}
