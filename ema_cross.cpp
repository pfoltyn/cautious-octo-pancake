#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

#include "ema_cross.h"
#include "spdlog/spdlog.h"

#pragma warning(push, 0)
#include "ta_libc.h"
#pragma warning(pop)

namespace ema_cross
{

constexpr double TX_COST = 0.02;
constexpr double DEC_SIZE = 0.01;
constexpr double INC_SIZE = 0.04;

strategy_error_e strategy_c::init(const strategy_data_s& data)
{
    constexpr int overlap = 400;
    int start_idx = (data.start_idx < overlap) ? 0 : data.start_idx - overlap;

    int slow_begin_idx = 0;
    int slow_skip_idx = 0;
    int slow_nb_element = 0;

    int fast_begin_idx = 0;
    int fast_skip_idx = 0;
    int fast_nb_element = 0;

    int size = data.end_idx - start_idx;
    if (ema_fast.size() < size) {
        ema_fast.resize(size);
    }
    if (ema_slow.size() < size) {
        ema_slow.resize(size);
    }

    TA_RetCode ta_ret = TA_EMA(
        start_idx,                     /* startIdx */
        data.end_idx,                  /* endIdx */
        data.close.data(),             /* inReal[] */
        param.slow_period,             /* tInPeriod From 2 to 100000 */
        &slow_begin_idx,               /* outBegIdx */
        &slow_nb_element,              /* outNBElement */
        ema_slow.data()                /* outEmaFast[] */
    );
    if (ta_ret != TA_SUCCESS) {
        spdlog::critical("TA_EMA(slow) failed with error: {0:d};  hex: {0:x}", ta_ret);
        return strategy_error_e::Error;
    }

    slow_skip_idx = (slow_begin_idx >= data.start_idx) ? 0 : data.start_idx - slow_begin_idx + 1;

    ta_ret = TA_EMA(
        start_idx,                     /* startIdx */
        data.end_idx,                  /* endIdx */
        data.close.data(),             /* inReal[] */
        param.fast_period,             /* tInPeriod From 2 to 100000 */
        &fast_begin_idx,               /* outBegIdx */
        &fast_nb_element,              /* outNBElement */
        ema_fast.data()                /* outEmaFast[] */
    );
    if (ta_ret != TA_SUCCESS) {
        spdlog::critical("TA_EMA(fast) failed with error: {0:d};  hex: {0:x}", ta_ret);
        return strategy_error_e::Error;
    }

    fast_skip_idx = (fast_begin_idx >= data.start_idx) ? 0 : data.start_idx - fast_begin_idx + 1;

    if (slow_begin_idx > fast_begin_idx) {
        begin_idx = slow_begin_idx;
        fast_skip_idx += slow_begin_idx - fast_begin_idx;
    }
    else {
        begin_idx = fast_begin_idx;
        slow_skip_idx += fast_begin_idx - slow_begin_idx;
    }
    nb_element = std::min(slow_nb_element, fast_nb_element);

    return strategy_error_e::Success;
}

strategy_error_e strategy_c::run(strategy_result_s& result, const strategy_log_e log = strategy_log_e::All)
{
    for (int idx = data.skip; idx < data.nb_element - 1; ++idx) {
        int price_idx = data.begin_idx + idx + 1;

        if (data.low[price_idx] < data.ll[idx]) {
            if ((data.macd[idx] > data.signal[idx])) {
                if (data.depo <= DEPO_MIN) {
                    continue;
                }
                double chunk = data.depo * INC_SIZE;
                double pos_chunk = chunk / (data.ll[idx] * (1 + TX_COST));
                data.pos += pos_chunk;
                data.depo -= chunk;
                data.tx_cnt++;
                idx += params.timeout - 1; // timeout
                if (log) {
                    printf("%d,%d,%d,%d,%.4f,%.4f,%.4f\n",
                        params.fast_period, params.slow_period, params.sig_period, params.timeout,
                        data.depo, data.pos, data.depo + (data.pos * data.close[price_idx]));
                }
            }
        }
        if (data.high[price_idx] > data.pp[idx]) {
            if ((data.macd[idx] < data.signal[idx])) {
                if (data.pos <= POS_MIN) {
                    continue;
                }
                double chunk = data.pos * DEC_SIZE;
                double depo_chunk = chunk * (data.ll[idx] * (1 - TX_COST));
                data.depo += depo_chunk;
                data.pos -= chunk;
                data.tx_cnt++;
                idx += params.timeout - 1; //timeout
                if (log) {
                    printf("%d,%d,%d,%d,%.4f,%.4f,%.4f\n",
                        params.fast_period, params.slow_period, params.sig_period, params.timeout,
                        data.depo, data.pos, data.depo + (data.pos * data.close[price_idx]));
                }
            }
        }
    }
}

strategy_error_e strategy_c::update()
{
    TA_RetCode ta_ret = TA_SUCCESS;
    double max_money = 0;
    params_s new_par = params;
    for (int fast = -2; fast < 3; ++fast) {
        if (params.fast_period + fast < 2) {
            continue;
        }
        for (int slow = -2; slow < 3; ++slow) {
            if (params.slow_period + slow <= params.fast_period + fast) {
                continue;
            }
            for (int sig = -2; sig < 3; ++sig) {
                if (params.sig_period + sig < 1) {
                    continue;
                }
                for (int tim = -2; tim < 3; ++tim) {
                    if (params.timeout + tim < 1) {
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
                    if (data.depo + (data.pos * data.close[data.end_idx]) > max_money) {
                        max_money = data.depo + (data.pos * data.close[data.end_idx]);
                        new_par = current;
                    }
                    data.depo = depo;
                    data.pos = pos;
                    data.tx_cnt = tx_cnt;
                }
            }
        }
    }
    params = new_par;
    return ta_ret;
}

} // namespace ema_cross
