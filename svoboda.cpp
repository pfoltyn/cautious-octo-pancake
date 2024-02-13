#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#pragma warning(push, 0)
#include "ta_libc.h"
#pragma warning(pop)

constexpr double TX_COST = 0.02;
constexpr double DEC_SIZE = 0.01;
constexpr double INC_SIZE = 0.04;

constexpr double DEPOSIT = 100000.0;
constexpr double POS_MIN = 0.01;
constexpr double DEPO_MIN = 100;

struct data_s
{
    double depo;
    double pos;
    int tx_cnt;

    std::vector<std::string> timestamp;
    std::vector<double> open;
    std::vector<double> high;
    std::vector<double> low;
    std::vector<double> close;

    int start_idx;
    int end_idx;
    int nb_element;

    std::vector<double> macd;
    std::vector<double> signal;
    std::vector<double> histogram;

    std::vector<double> pp;
};

struct params_s
{
    int fast_period;
    int slow_period;
    int sig_period;

    int pp_period;
};

std::string get_file_contents(const std::string filename)
{
    std::ifstream in(filename, std::ios::in | std::ios::binary);
    if (in)
    {
        std::string contents;
        in.seekg(0, std::ios::end);
        contents.resize(in.tellg());
        in.seekg(0, std::ios::beg);
        in.read(&contents[0], contents.size());
        in.close();
        return(contents);
    }
    throw(errno);
}

data_s parse_data(std::string data)
{
    data_s result = {};
    bool skip_header = true;
    std::stringstream ss(std::move(data));
    std::string line;
    while (std::getline(ss, line, '\n')) {
        if (skip_header) {
            skip_header = false;
            continue;
        }
        std::stringstream line_stream(line);
        std::string cell;
        int cell_cnt = 5;
        while (std::getline(line_stream, cell, ',') && cell_cnt > 0) {
            switch (cell_cnt)
            {
            case 5:
                result.timestamp.push_back(cell);
                break;
            case 4:
                result.open.push_back(std::stod(cell));
                break;
            case 3:
                result.high.push_back(std::stod(cell));
                break;
            case 2:
                result.low.push_back(std::stod(cell));
                break;
            case 1:
                result.close.push_back(std::stod(cell));
                break;
            default:
                break;
            }
            cell_cnt--;
        }
    }
    return result;
}

TA_RetCode calculations(data_s& data, const params_s& params)
{
    size_t start_idx = (data.start_idx > 32) ? data.start_idx - 33 : data.start_idx;

    data.pp.resize(data.close.size());
    auto low_begin = data.low.begin() + start_idx;
    auto min_elem = std::min_element(low_begin, low_begin + params.pp_period);
    auto high_begin = data.high.begin() + start_idx;
    auto max_elem = std::max_element(high_begin, high_begin + params.pp_period);
    for (size_t idx = start_idx + params.pp_period; idx < data.end_idx; ++idx) {
        auto itr = data.close.begin() + idx;
        auto low_itr = data.low.begin() + idx;
        auto high_itr = data.high.begin() + idx;
        if (low_itr - params.pp_period > min_elem) {
            min_elem = std::min_element(low_itr - params.pp_period, low_itr);
        }
        else if (*(low_itr - 1) < *min_elem) {
            min_elem = low_itr - 1;
        }
        if (high_itr - params.pp_period > max_elem) {
            max_elem = std::max_element(high_itr - params.pp_period, high_itr);
        }
        else if (*(high_itr - 1) > *max_elem) {
            max_elem = high_itr - 1;
        }
        data.pp[idx - start_idx + params.pp_period] = ((*max_elem + *min_elem + *(itr - 1)) / 3) + ((*max_elem - *min_elem) * 0.618);
    }

    data.macd.resize(data.close.size());
    data.signal.resize(data.close.size());
    data.histogram.resize(data.close.size());

    TA_RetCode ta_ret = TA_MACD(
        data.start_idx,        /* startIdx */
        data.end_idx,          /* endIdx */
        data.close.data(),     /* inReal[] */
        params.fast_period,    /* tInFastPeriod From 2 to 100000 */
        params.slow_period,    /* optInSlowPeriod From 2 to 100000 */
        params.sig_period,     /* optInSignalPeriod From 1 to 100000 */
        &data.start_idx,       /* outBegIdx */
        &data.nb_element,      /* outNBElement */
        data.macd.data(),      /* outMACD[] */
        data.signal.data(),    /* outMACDSignal[] */
        data.histogram.data()  /* outMACDHist[] */
    );

    if (ta_ret != TA_SUCCESS) {
        std::cout << data.start_idx << ","
            << data.end_idx << ","
            << data.close.size() << ","
            << params.fast_period << ","
            << params.slow_period << ","
            << params.sig_period << std::endl;

        std::cout << "TA_MACD() failed with error " << ta_ret << std::endl;
    }

    return ta_ret;
}

void strategy(data_s& data, const params_s& params, bool log = true)
{
    int timeout = 0;
    for (size_t idx = 0; idx < data.nb_element; ++idx) {
        if (timeout > 0) {
            timeout--;
        }
        if (data.start_idx + idx >= data.end_idx) {
            break;
        }
        if ((timeout == 0) && data.high[data.start_idx + idx] > data.pp[idx]) {
            timeout = 12;
            if (data.macd[idx] > data.signal[idx]) {
                if (data.depo <= DEPO_MIN) {
                    continue;
                }
                double chunk = data.depo * INC_SIZE;
                double pos_chunk = chunk / (data.pp[idx] * (1 + TX_COST));
                data.pos += pos_chunk;
                data.depo -= chunk;
                data.tx_cnt++;
            }
            else {
                if (data.pos <= POS_MIN) {
                    continue;
                }
                double chunk = data.pos * DEC_SIZE;
                double depo_chunk = chunk * (data.pp[idx] * (1 - TX_COST));
                data.depo += depo_chunk;
                data.pos -= chunk;
                data.tx_cnt++;
            }
            if (log) {
                printf("%d,%d,%d,%.4f,%.4f,%.4f\n", params.fast_period, params.slow_period, params.sig_period, data.depo, data.pos, data.depo + (data.pos * data.close[idx]));
            }
        }
    }
}

TA_RetCode update_params(data_s& data, params_s& params)
{
    TA_RetCode ta_ret = TA_SUCCESS;
    double max_money = 0;
    params_s new_par = params;
    for (int fast = 0; fast < 3; ++fast) {
        if (params.fast_period + fast < 2) {
            continue;
        }
        for (int slow = 0; slow < 3; ++slow) {
            if (params.slow_period + slow <= params.fast_period + fast) {
                continue;
            }
            for (int sig = 0; sig < 3; ++sig) {
                if (params.sig_period + sig < 1) {
                    continue;
                }

                params_s current = params;
                current.fast_period += fast;
                current.slow_period += slow;
                current.sig_period += sig;

                ta_ret = calculations(data, current);
                if (ta_ret != TA_SUCCESS) {
                    std::cout << "calculations() failed with error " << ta_ret << std::endl;
                    return ta_ret;
                }

                double depo = data.depo;
                double pos = data.pos;
                int tx_cnt = data.tx_cnt;
                strategy(data, current, false /*log*/);
                if (data.depo + (data.pos * data.close.back()) > max_money) {
                    max_money = data.depo + (data.pos * data.close.back());
                    new_par = current;
                }
                data.depo = depo;
                data.pos = pos;
                data.tx_cnt = tx_cnt;
            }
        }
    }
    params = new_par;
    return ta_ret;
}

int main()
{
    const std::string filename = "C:\\Users\\piotr\\Desktop\\svoboda\\BTCUSDT-1h-data.csv";
    std::string file_contents = get_file_contents(filename);
    data_s data = parse_data(file_contents);

    TA_RetCode ta_ret = TA_Initialize();
    if (ta_ret != TA_SUCCESS) {
        std::cout << "TA_Initialize() failed with error " << ta_ret << std::endl;
        return -1;
    }

    data.depo = 100000;
    data.pos = 0.0;
    data.tx_cnt = 0;

    params_s params = { 12, 26, 9, 27 };
    int interval = 24 * 31;

    std::ofstream fout("data31.dat", std::ios::out | std::ios::binary);
    for (int start_idx = 100; start_idx < data.close.size(); start_idx += interval) {
        data.start_idx = start_idx;
        data.end_idx = start_idx + interval;

        if (data.end_idx >= data.close.size()) {
            data.end_idx = static_cast<int>(data.close.size());
        }

        ta_ret = calculations(data, params);
        if (ta_ret != TA_SUCCESS) {
            std::cout << "calculations() failed with error " << ta_ret << std::endl;
            return -1;
        }
        
        for (int iii = 0; iii < data.nb_element; ++iii) {
            std::string ggg = std::to_string(data.signal[iii]);
            fout.write(ggg.c_str(), ggg.length());
            fout.write("\n", 1);
        }
        fout.write("\n", 1);

        strategy(data, params, true /*log*/);

        //ta_ret = update_params(data, params);
        if (ta_ret != TA_SUCCESS) {
            std::cout << "update_params() failed with error " << ta_ret << std::endl;
            return -1;
        }
    }


    fout.close();

    TA_Shutdown();
    return 0;
}