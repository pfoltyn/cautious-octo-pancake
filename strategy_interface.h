#pragma once

#include <vector>
#include <string>

constexpr double DEPOSIT = 100000.0;
constexpr double POS_MIN = 0.01;
constexpr double DEPO_MIN = 100;

enum class strategy_error_e
{
	Success,
	Error
};

enum class strategy_log_e
{
	None,
	All,
};

struct strategy_data_s
{
    std::vector<std::string> timestamp;
    std::vector<double> open;
    std::vector<double> high;
    std::vector<double> low;
    std::vector<double> close;

    size_t start_idx;
    size_t end_idx;
};

struct strategy_result_s
{
    double depo;
    double pos;
    int tx_cnt;
};

class strategy_interface_c
{
public:
	virtual strategy_error_e init(const strategy_data_s& data) = 0;
	virtual strategy_error_e run(strategy_result_s& result, const strategy_log_e log = strategy_log_e::All) = 0;
	virtual strategy_error_e update() = 0;
};
