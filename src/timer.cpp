#include "timer.h"

namespace sirius
{

std::map<std::string, Timer*> ftimers;

std::map<std::string, std::vector<double> > Timer::timers_;
std::map<std::string, std::vector<double> > Timer::global_timers_;

void Timer::start()
{
    if (active_)
    {
        printf("timer %s is already running\n", label_.c_str());
        Platform::abort();
    }

    gettimeofday(&starting_time_, NULL);
    active_ = true;
}

void Timer::stop()
{
    if (!active_)
    {
        printf("timer %s was not running\n", label_.c_str());
        Platform::abort();
    }

    timeval end;
    gettimeofday(&end, NULL);

    double val = double(end.tv_sec - starting_time_.tv_sec) + 
                 double(end.tv_usec - starting_time_.tv_usec) / 1e6;
    
    switch (timer_type_)
    {
        case _local_timer_:
        {
            timers_[label_].push_back(val);
            break;
        }
        case _global_timer_:
        {
            global_timers_[label_].push_back(val);
            break;
        }
    }

    active_ = false;
}

double Timer::value()
{
    if (active_)
    {
        std::stringstream s;
        s << "timer " << label_ << " is active";
        error_local(__FILE__, __LINE__, s);
    }
    std::vector<double> values;
    switch (timer_type_)
    {
        case _local_timer_:
        {
            values = timers_[label_];
            break;
        }
        case _global_timer_:
        {
            values = global_timers_[label_];
            break;
        }
    }
    double d = 0;
    for (int i = 0; i < (int)values.size(); i++) d += values[i];
    return d;
}

extern "C" void print_cuda_timers();

void Timer::print()
{
    std::map< std::string, timer_stats> tstats = collect_timer_stats();
    
    if (Platform::mpi_rank() == 0)
    {
        printf("\n");
        printf("Timers\n");
        for (int i = 0; i < 115; i++) printf("-");
        printf("\n");
        printf("name                                                              count      total        min        max    average\n");
        for (int i = 0; i < 115; i++) printf("-");
        printf("\n");

        std::map<std::string, timer_stats>::iterator it;
        for (it = tstats.begin(); it != tstats.end(); it++)
        {
            auto ts = it->second;
            if (ts.timer_type == _local_timer_)
            {
                printf("%-60s :    %5i %10.4f %10.4f %10.4f %10.4f\n", it->first.c_str(), ts.count, ts.total_value, 
                       ts.min_value, ts.max_value, ts.average_value);
            }
            if (ts.timer_type == _global_timer_)
            {
                printf("%-60s :    %5i %10.4f %10.4f %10.4f %10.4f +\n", it->first.c_str(), ts.count, ts.total_value, 
                       ts.min_value, ts.max_value, ts.average_value);
            }
        }
        
        #ifdef _GPU_
        print_cuda_timers();
        #endif
    }
}

void Timer::delay(double dsec)
{
    timeval t1;
    timeval t2;
    double d;

    gettimeofday(&t1, NULL);
    do
    {
        gettimeofday(&t2, NULL);
        d = double(t2.tv_sec - t1.tv_sec) + double(t2.tv_usec - t1.tv_usec) / 1e6;
    } while (d < dsec);
}

std::map< std::string, timer_stats> Timer::collect_timer_stats()
{
    std::map< std::string, timer_stats> tstats;

    //std::map<std::string, std::vector<double> >::iterator it;

    /* collect local timers */
    for (auto& it: timers_)
    {
        timer_stats ts;

        ts.timer_type = _local_timer_;
        ts.count = (int)it.second.size();
        ts.total_value = 0.0;
        ts.min_value = 1e100;
        ts.max_value = 0.0;
        for (int i = 0; i < ts.count; i++)
        {
            ts.total_value += it.second[i];
            ts.min_value = std::min(ts.min_value, it.second[i]);
            ts.max_value = std::max(ts.max_value, it.second[i]);
        }
        ts.average_value = (ts.count == 0) ? 0.0 : ts.total_value / ts.count;
        if (ts.count == 0) ts.min_value = 0.0;

        tstats[it.first] = ts;
    }

    /* collect and broadcast global timer labels from rank#0 */
    std::vector< std::string > labels;
    std::vector<int> label_sizes;
    std::vector<char> label_str;
    if (Platform::mpi_rank() == 0)
    {
        for (auto& it: global_timers_)
        {
            /* save timer's label */
            labels.push_back(it.first);
            /* save length of the label */
            label_sizes.push_back((int)it.first.size());
            /* save label in the single array */ 
            for (int i = 0; i < (int)it.first.size(); i++) label_str.push_back(it.first[i]);
        }
    }

    // TODO: this can be moved to Platform::bcast

    /* broadcast the number of labels from rank#0 */
    int n = (int)label_sizes.size();
    Platform::bcast(&n, 1, 0);
    /* each MPI rank allocates space for label sizes */ 
    if (Platform::mpi_rank() != 0) label_sizes.resize(n);
    /* broadacst label sizes from rank#0 */
    Platform::bcast(&label_sizes[0], n, 0);
    
    /* broadcast the size of labels buffer from rank#0 */
    n = (int)label_str.size();
    Platform::bcast(&n, 1, 0);
    /* allocate space for labels buffer */
    if (Platform::mpi_rank() != 0) label_str.resize(n);
    /* broadcast labels buffer */
    Platform::bcast(&label_str[0], n, 0);
    
    /* construct list of labels exactly like on rank#0 */
    if (Platform::mpi_rank() != 0)
    {
        int offset = 0;
        for (int sz: label_sizes)
        {
            labels.push_back(std::string(&label_str[offset], sz));
            offset += sz;
        }
    }

    /* now all MPI ranks loop over the same sequence of global timer labels */
    for (auto& label: labels)
    {
        timer_stats ts;

        ts.timer_type = _global_timer_;

        /* this MPI rank doesn't have a corresponding timer */
        if (global_timers_.count(label) == 0)
        {
            ts.count = 0;
            ts.total_value = 0.0;
            ts.min_value = 0.0;
            ts.max_value = 0.0;
            ts.average_value = 0.0;
        }
        else
        {
            ts.count = (int)global_timers_[label].size();
            ts.total_value = 0.0;
            ts.min_value = 1e100;
            ts.max_value = 0.0;
            /* loop over timer measurements and collect total, min, max, average */
            for (int k = 0; k < ts.count; k++)
            {
                double v = global_timers_[label][k];
                ts.total_value += v;
                ts.min_value = std::min(ts.min_value, v);
                ts.max_value = std::max(ts.max_value, v);
            }
            ts.average_value = (ts.count == 0) ? 0.0 : ts.total_value / ts.count;
            if (ts.count == 0) ts.min_value = 0.0;
        }
        
        /* collect timer counts from all ranks */
        std::vector<int> counts(Platform::num_mpi_ranks());
        counts[Platform::mpi_rank()] = ts.count;
        Platform::allgather(&counts[0], Platform::mpi_rank(), 1);
        
        /* collect timer statistics from all ranks */
        std::vector<double> values(4 * Platform::num_mpi_ranks());
        values[4 * Platform::mpi_rank() + 0] = ts.total_value;
        values[4 * Platform::mpi_rank() + 1] = ts.min_value;
        values[4 * Platform::mpi_rank() + 2] = ts.max_value;
        values[4 * Platform::mpi_rank() + 3] = ts.average_value;

        Platform::allgather(&values[0], 4 * Platform::mpi_rank(), 4);

        double max_total_value = 0;
        double total_value = 0;
        int total_count = 0;
        for (int k = 0; k < Platform::num_mpi_ranks(); k++)
        {
            /* maximum total value across all ranks */
            max_total_value = std::max(max_total_value, values[4 * k + 0]);
            /* minimum value across all ranks */
            ts.min_value = std::min(ts.min_value, values[4 * k + 1]);
            /* maximum value across all ranks */
            ts.max_value = std::max(ts.max_value, values[4 * k + 2]);
            /* total global value */
            total_value += values[4 * k + 0];
            /* total number of counts */
            total_count += counts[k];
        }
        /* report maximum total value across all ranks */
        ts.total_value = max_total_value;
        ts.average_value = (total_count == 0) ? 0.0 : total_value / total_count;

        tstats[label] = ts;
    }

    return tstats;
}

};

