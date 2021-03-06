#pragma once

#include <catboost/libs/algo/plot.h>

#include <util/generic/noncopyable.h>

#include <Python.h>

class TGilGuard : public TNonCopyable {
public:
    TGilGuard()
        : State_(PyGILState_Ensure())
    { }

    ~TGilGuard() {
        PyGILState_Release(State_);
    }
private:
    PyGILState_STATE State_;
};

void ProcessException();
void SetPythonInterruptHandler();
void ResetPythonInterruptHandler();

TVector<TVector<double>> EvalMetrics(
    const TFullModel& model,
    const TPool& pool,
    const TVector<TString>& metricsDescription,
    int begin,
    int end,
    int evalPeriod,
    int threadCount,
    const TString& tmpDir
);


class TMetricsPlotCalcerPythonWrapper {
public:
    TMetricsPlotCalcerPythonWrapper(const TVector<TString>& metricDescriptions,
                                    const TFullModel& model,
                                    int begin,
                                    int end,
                                    int evalPeriod,
                                    int threadCount,
                                    const TString& tmpDir,
                                    bool deleteTempDirOnExit = false)
    : MetricPlotCalcer(CreateMetricCalcer(
            model,
            metricDescriptions,
            begin,
            end,
            evalPeriod,
            Executor,
            tmpDir,
            &Metrics)) {

        Executor.RunAdditionalThreads(threadCount - 1);
        MetricPlotCalcer.SetDeleteTmpDirOnExit(deleteTempDirOnExit);
    }

    ~TMetricsPlotCalcerPythonWrapper() {
        MetricPlotCalcer.ClearTempFiles();
    }

    void AddPool(const TPool& pool) {
        MetricPlotCalcer.ProceedDataSet(pool);
    }

    TVector<const IMetric*> GetMetricRawPtrs() const {
        TVector<const IMetric*> ptrs;
        for (const auto& metric : Metrics) {
            ptrs.push_back(metric.Get());
        }
        return ptrs;
    }

    TVector<TVector<double>> ComputeScores()  {
        return MetricPlotCalcer.GetMetricsScore();
    }

private:
    NPar::TLocalExecutor Executor;
    TVector<THolder<IMetric>> Metrics;
    TMetricsPlotCalcer MetricPlotCalcer;
};
