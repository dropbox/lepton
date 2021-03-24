#include "../util/memory.hh"
#include <assert.h>
#ifdef _WIN32
#include <io.h>
#else
#include <unistd.h>
#endif
#include <fstream>
#include <iostream>

#ifdef __aarch64__
#define USE_SCALAR 1
#endif

#ifndef USE_SCALAR
#include <emmintrin.h>
#endif

#include "model.hh"
bool all_branches_identity(const Branch * start, const Branch * end) {
    for (const Branch * i = start;i != end; ++i) {
        if (!i->is_identity()){
            return false;
        }
    }
    return true;
}
void set_branch_range_identity(Branch * start, Branch * end) {
    if (__builtin_expect(end - start <= 64, 0)) {
        for (;start != end; ++start) {
            start->set_identity();
        }
        return;
    }
#if defined(__AVX__) && !defined(USE_SCALAR)
    for (int i = 0;i < 32; ++i) {
        start[i].set_identity();
    }
    for (int i = 1; i <= 32; ++i) {
        end[-i].set_identity();
    }
    char * data = (char *)(void*)start;
    __m256i r0 = _mm256_loadu_si256((const __m256i*)data);
    __m256i r1 = _mm256_loadu_si256((const __m256i*)(data + 32));
    __m256i r2 = _mm256_loadu_si256((const __m256i*)(data + 64));
    size_t offset = data - (char*)0;
    size_t align = 32 - (offset % 32);
    char * dataend = (char*)end;
    size_t offsetend = dataend - (char*)0;
    __m256i *write_end = (__m256i*)(dataend - (offsetend % 32));
    __m256i *write_cursor = (__m256i*)(data + align);
    switch(align % 3) {
        case 2:
            _mm256_store_si256(write_cursor, r1);
            write_cursor += 1;
        case 1:
            _mm256_store_si256(write_cursor, r2);
            write_cursor += 1;
        case 0:
            break;
    }
    while(write_cursor + 2 < write_end) {
        _mm256_store_si256(write_cursor, r0);
        _mm256_store_si256(write_cursor + 1, r1);
        _mm256_store_si256(write_cursor + 2, r2);
        write_cursor += 3;
    }

#elif defined(__SSE2__) && !defined(USE_SCALAR)
    for (int i = 0;i < 16; ++i) {
        start[i].set_identity();
    }
    for (int i = 1; i <= 16; ++i) {
        end[-i].set_identity();
    }
    char * data = (char *)(void*)start;
    __m128i r0 = _mm_loadu_si128((const __m128i*)data);
    __m128i r1 = _mm_loadu_si128((const __m128i*)(data + 16));
    __m128i r2 = _mm_loadu_si128((const __m128i*)(data + 32));
    size_t offset = data - (char*)0;
    size_t align = 16 - (offset % 16);
    char * dataend = (char*)end;
    size_t offsetend = dataend - (char*)0;
    __m128i *write_end = (__m128i*)(dataend - (offsetend % 16));
    __m128i *write_cursor = (__m128i*)(data + align);
    switch(align % 3) {
        case 1:
            _mm_store_si128(write_cursor, r1);
            write_cursor += 1;
        case 2:
            _mm_store_si128(write_cursor, r2);
            write_cursor += 1;
        case 0:
            break;
    }
    while(write_cursor + 2 < write_end) {
        _mm_store_si128(write_cursor, r0);
        _mm_store_si128(write_cursor + 1, r1);
        _mm_store_si128(write_cursor + 2, r2);
        write_cursor += 3;
    }
#else
    for (;start != end; ++start) {
        start->set_identity();
    }
#endif
    dev_assert(all_branches_identity(start, end));
}

#ifdef _WIN32
__declspec(align(16))
#endif
int32_t ProbabilityTablesBase::icos_idct_edge_8192_dequantized_x_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
__attribute__((aligned(16)))
#endif
    = {{0}};

#ifdef _WIN32
__declspec(align(16))
#endif
int32_t ProbabilityTablesBase::icos_idct_edge_8192_dequantized_y_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
__attribute__((aligned(16)))
#endif
  = {{0}};
#ifdef _WIN32
__declspec(align(16))
#endif
int32_t ProbabilityTablesBase::icos_idct_linear_8192_dequantized_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
__attribute__((aligned(16)))
#endif
   = {{0}};
#ifdef ANNOTATION_ENABLED
Context *gctx = (Context*)memset(calloc(sizeof(Context),1), 0xff, sizeof(Context));
#endif

#ifdef _WIN32
__declspec(align(16))
#endif
uint16_t ProbabilityTablesBase::quantization_table_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
__attribute__((aligned(16)))
#endif
    ;
#ifdef _WIN32
__declspec(align(16))
#endif
uint16_t ProbabilityTablesBase::freqmax_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
__attribute__((aligned(16)))
#endif
   ;

#ifdef _WIN32
__declspec(align(16))
#endif
uint8_t ProbabilityTablesBase::min_noise_threshold_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
__attribute__((aligned(16)))
#endif
    ;

#ifdef _WIN32
__declspec(align(16))
#endif
uint8_t ProbabilityTablesBase::bitlen_freqmax_[(int)ColorChannel::NumBlockTypes][64]
#ifndef _WIN32
    __attribute__ ((aligned (16)))
#endif
    ;
int get_sum_median_8(int16_t *dc_estimates) {
    int len_est = 16;
    int min_dc, max_dc;
    for (int start = 0; start < 4; ++start) {
        if (dc_estimates[start] > dc_estimates[len_est - 1 - start]) {
            std::swap(dc_estimates[start], dc_estimates[len_est - 1 - start]);
        }
        min_dc = dc_estimates[start];
        max_dc = dc_estimates[len_est - 1 - start];
        int min_idx = start;
        int max_idx = len_est - 1 - start;
        for (int i = start + 1; i < len_est - start - 1; ++i) {
            if (dc_estimates[i] > max_dc) {
                max_idx = i;
                max_dc = dc_estimates[i];
            }
            if (dc_estimates[i] < min_dc) {
                min_idx = i;
                min_dc = dc_estimates[i];
            }
        }
        dc_estimates[min_idx] = dc_estimates[start];
        dc_estimates[max_idx] = dc_estimates[len_est - 1 - start];
        dc_estimates[start] = min_dc;
        dc_estimates[len_est - 1 - start] = max_dc;
    }
    int sum = 0;
    for (int i = 4; i < len_est - 4; ++i) {
        sum += dc_estimates[i];
    }
    return sum;
}
void serialize_model(const Model & model, int output_fp )
{
    size_t left_to_write = sizeof(model);
    const char * data = reinterpret_cast<const char*>( &model );
    while(left_to_write) {
        size_t written;
#ifdef _WIN32
        written = _write(output_fp, data, left_to_write);
#else
        written = write(output_fp, data, left_to_write);
#endif
        if (written <= 0) {
            if (errno != EINTR) {
                break;
            }
        }
        left_to_write -= written;
        data += written;
    }
}

void optimize_model(Model &model)
{
    (void)model;
    //model.forall( [&] ( Branch & x ) { x.optimize(); } );
}


bool filter(const Branch& a,
            const Branch* b) {
#ifndef USE_COUNT_FREE_UPDATE
    if (a.true_count() == 0 && a.false_count() == 0) {
        return false;
    }
    if (b) {
        if (a.prob() + 1 == b->prob() ||
            a.prob() == b->prob() + 1 ||
            a.prob() == b->prob()) {
            return false;
        }
    } else {
        return a.true_count () > 300 && a.false_count() > 300;
    }
#endif
    return true;
}
template<class BranchArray> void print_helper(const BranchArray& ba,
                                              const BranchArray* other,
                                              const std::string &table_name,
                                              const std::vector<std::string> &names,
                                              std::vector<uint32_t> &values,
                                              Model::PrintabilitySpecification print_branch_bitmask) {
    values.push_back(0);
    for (size_t i = 0; i < ba.dimsize(); ++i) {
        values.back() = i;
        auto subarray = ba.at(i);
        auto otherarray = &subarray;
        otherarray= nullptr;
        print_helper(subarray, otherarray, table_name, names, values, print_branch_bitmask);
    }
    values.pop_back();
}

bool is_printable(uint64_t true_count, uint64_t false_count,
                  double true_false_ratio, double other_ratio, bool other,
                  Model::PrintabilitySpecification spec) {
    if (other) {
        if (true_count + false_count >= spec.min_samples) {
            double delta = true_false_ratio - other_ratio;
            if (delta < 0) delta = -delta;
            if (delta < spec.tolerance) {
                return (Model::CLOSE_TO_ONE_ANOTHER & spec.printability_bitmask) ? true : false;
            } else {
                return (Model::PRINTABLE_OK & spec.printability_bitmask) ? true : false;
            }
        } else {
            return (Model::PRINTABLE_INSIGNIFICANT & spec.printability_bitmask) ? true : false;
        }
    } else {
        if (true_count + false_count >= spec.min_samples) {
            double delta = true_false_ratio - .5;
            if (delta < 0) delta = -delta;
            if (delta < spec.tolerance) {
                return (Model::CLOSE_TO_50 & spec.printability_bitmask) ? true : false;
            } else {
                return (Model::PRINTABLE_OK & spec.printability_bitmask) ? true : false;
            }
        } else {
            return (Model::PRINTABLE_INSIGNIFICANT & spec.printability_bitmask) ? true : false;
        }
    }
}
template<> void print_helper(const Branch& ba,
                             const Branch* other,
                             const std::string&table_name,
                             const std::vector<std::string> &names,
                             std::vector<uint32_t> &values,
                             Model::PrintabilitySpecification print_branch_bitmask) {
#ifndef USE_COUNT_FREE_UPDATE
    double ratio = (ba.true_count() + 1) / (double)(ba.false_count() + ba.true_count() + 2);
    (void) ratio;
    double other_ratio = ratio;
    if (other) {
        other_ratio = (other->true_count() + 1) / (double)(other->false_count() + other->true_count() + 2);
    }
    (void) other_ratio;
    if (ba.true_count() > 0 ||  ba.false_count() > 1) {
        if (is_printable(ba.true_count(), ba.false_count(), ratio, other_ratio, !!other, print_branch_bitmask))
        {
            always_assert(names.size() == values.size());
            std::cout <<table_name<<"::";
            for (size_t i = 0; i < names.size(); ++i) {
                std::cout << names[i]<<'['<<values[i]<<']';
            }
            std::cout << " = (" << ba.true_count() <<", "<<  (ba.false_count() - 1) << ")";
            if (other) {
                std::cout << " = (" << other->true_count() <<", "<<  (other->false_count() - 1) << "}";
            }
            std::cout << std::endl;
        }
    }
#endif
}
template<class BranchArray> void print_all(const BranchArray &ba,
                                           const BranchArray *other_ba,
                                           const std::string &table_name,
                                           const std::vector<std::string> &names,
                                           Model::PrintabilitySpecification spec) {
    std::vector<uint32_t> tmp;
    print_helper(ba, other_ba, table_name, names, tmp, spec);
}

const Model &Model::debug_print(const Model * other,
                                                        Model::PrintabilitySpecification spec)const
{
#ifndef _WIN32
    print_all(this->num_nonzeros_counts_7x7_,
              other ? &other->num_nonzeros_counts_7x7_ : nullptr,
              "NONZERO 7x7",
              {"cmp","nbr","bit","prevbits"}, spec);
    
    print_all(this->num_nonzeros_counts_1x8_,
              other ? &other->num_nonzeros_counts_1x8_ : nullptr,
              "NONZERO_1x8",
              {"cmp","eobx","num_nonzeros","bit","prevbits"}, spec);
    print_all(this->num_nonzeros_counts_8x1_,
              other ? &other->num_nonzeros_counts_8x1_ : nullptr,
              "NONZERO_8x1",
              {"cmp","eobx","num_nonzeros","bit","prevbits"}, spec);
    print_all(this->exponent_counts_dc_,
              other ? &other->exponent_counts_dc_ : nullptr,
              "EXP_DC",
              {"cmp","num_nonzeros","neigh_exp","bit","prevbits"}, spec);
    print_all(this->exponent_counts_,
              other ? &other->exponent_counts_ : nullptr,
              "EXP7x7",
              {"cmp","coef","num_nonzeros","neigh_exp","bit","prevbits"}, spec);
    print_all(this->exponent_counts_x_,
              other ? &other->exponent_counts_x_: nullptr,
              "EXP_8x1",
              {"cmp","coef","num_nonzeros","neigh_exp","bit","prevbits"}, spec);
    print_all(this->residual_noise_counts_,
              other ? &other->residual_noise_counts_: nullptr,
              "NOISE",
              {"cmp","coef","num_nonzeros","bit"}, spec);
    print_all(this->residual_threshold_counts_,
              other ? &other->residual_threshold_counts_ : nullptr,
              "THRESH8",
              {"cmp","max","exp","prevbits"}, spec);
    print_all(this->sign_counts_,
              other ? &other->sign_counts_ : nullptr,
              "SIGN",
              {"cmp","lakh","exp"}, spec);
#endif
    return *this;
}

void normalize_model(Model& model) {
    model.forall( [&] ( Branch & x ) { x.normalize(); } );
}

void ProbabilityTablesBase::load_probability_tables()
{
    const char * model_name = getenv( "LEPTON_COMPRESSION_MODEL" );
    if (model_name) {
        const char * msg = "Using good probability tables!\n";
        while(write(2, msg, strlen(msg))< 0 && errno == EINTR) {
        }
        ProbabilityTables<true, BlockType::Y> model_tables(BlockType::Y, true, true, true);
        model_tables.load(*this, model_name);
        model_tables.normalize(*this);
    }
}

void reset_model(Model&model)
{
    model.forall( [&] ( Branch & x ) { x = Branch(); } );
}




void load_model(Model&model, const char * filename) {
    FILE * fp = fopen(filename, "rb");
    if (fp) {
        const size_t expected_size = fread(&model, 1, sizeof(model), fp);
        fclose(fp);
        (void)expected_size;
        always_assert(sizeof(model) == expected_size && "unexpected model file size.");
    } else {
        while(write(2, filename, strlen(filename))< 0 && errno == EINTR) {
        }
        const char * msg = " not found for input model\n";
        while(write(2, msg, strlen(msg))< 0 && errno == EINTR) {
        }
    }
}
