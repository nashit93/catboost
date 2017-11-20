#pragma once

#include "helpers.h"
#include <catboost/cuda/data/columns.h>
#include <catboost/cuda/ctrs/ctr.h>

#include <util/system/types.h>
#include <util/generic/map.h>
#include <util/generic/vector.h>
#include <util/generic/set.h>
#include <catboost/libs/model/hash.h>
#include <util/digest/multi.h>
#include <util/ysaveload.h>
#include <util/generic/algorithm.h>

namespace NCatboostCuda
{
    enum class EBinSplitType
    {
        TakeBin,
        TakeGreater
    };

    struct TBinarySplit
    {
        ui32 FeatureId = 0; //from feature manager
        ui32 BinIdx = 0;
        EBinSplitType SplitType;

        TBinarySplit(const ui32 featureId,
                     const ui32 binIdx,
                     EBinSplitType splitType)
                : FeatureId(featureId)
                , BinIdx(binIdx)
                , SplitType(splitType)
        {
        }

        TBinarySplit() = default;

        bool operator<(const TBinarySplit& other) const
        {
            return std::tie(FeatureId, BinIdx, SplitType) < std::tie(other.FeatureId, other.BinIdx, other.SplitType);
        }

        bool operator==(const TBinarySplit& other) const
        {
            return std::tie(FeatureId, BinIdx, SplitType) == std::tie(other.FeatureId, other.BinIdx, other.SplitType);
        }

        bool operator!=(const TBinarySplit& other) const
        {
            return !(*this == other);
        }

        ui64 GetHash() const
        {
            return MultiHash(FeatureId, BinIdx, SplitType);
        }

        Y_SAVELOAD_DEFINE(FeatureId, BinIdx, SplitType);
    };


    template<class TVectorType>
    inline void Unique(TVectorType& vector)
    {
        ui64 size = std::unique(vector.begin(), vector.end()) - vector.begin();
        vector.resize(size);
    }

    struct TFeatureTensor
    {
    public:
        bool IsSimple() const
        {
            return (Splits.size() + CatFeatures.size()) == 1;
        }

        TFeatureTensor& AddBinarySplit(const TBinarySplit& bin)
        {
            Splits.push_back(bin);
            SortUniqueSplits();
            return *this;
        }

        TFeatureTensor& AddBinarySplit(const TVector<TBinarySplit>& splits)
        {
            for (auto& bin : splits)
            {
                Splits.push_back(bin);
            }
            SortUniqueSplits();
            return *this;
        }

        void SortUniqueSplits()
        {
            Sort(Splits.begin(), Splits.end());
            Unique(Splits);
        }

        TFeatureTensor& AddCatFeature(const TVector<ui32>& featureIds)
        {
            for (auto feature : featureIds)
            {
                CatFeatures.push_back(feature);
            }
            SortUniqueCatFeatures();
            return *this;
        }

        TFeatureTensor& AddCatFeature(ui32 featureId)
        {
            CatFeatures.push_back(featureId);
            SortUniqueCatFeatures();
            return *this;
        }

        void SortUniqueCatFeatures()
        {
            Sort(CatFeatures.begin(), CatFeatures.end());
            Unique(CatFeatures);
        }

        TFeatureTensor& AddTensor(const TFeatureTensor& tensor)
        {
            for (auto& split : tensor.Splits)
            {
                Splits.push_back(split);
            }
            for (auto& catFeature : tensor.CatFeatures)
            {
                CatFeatures.push_back(catFeature);
            }
            SortUniqueSplits();
            SortUniqueCatFeatures();
            return *this;
        }

        bool operator==(const TFeatureTensor& other) const
        {
            return (Splits == other.GetSplits()) && (CatFeatures == other.GetCatFeatures());
        }

        bool operator!=(const TFeatureTensor& other) const
        {
            return !(*this == other);
        }

        bool IsEmpty() const
        {
            return CatFeatures.size() == 0 && Splits.size() == 0;
        }

        ui64 Size() const
        {
            return CatFeatures.size() + Splits.size();
        }

        ui64 GetHash() const
        {
            return MultiHash(TVecHash<TBinarySplit>()(Splits), VecCityHash(CatFeatures));
        }

        bool operator<(const TFeatureTensor& other) const
        {
            return std::tie(Splits, CatFeatures) < std::tie(other.Splits, other.CatFeatures);
        }

        bool IsSubset(const TFeatureTensor other) const
        {
            return NCatboostCuda::IsSubset(Splits, other.Splits) && NCatboostCuda::IsSubset(CatFeatures, other.CatFeatures);
        }

        const TVector<TBinarySplit>& GetSplits() const
        {
            return Splits;
        }

        const TVector<ui32>& GetCatFeatures() const
        {
            return CatFeatures;
        }

        ui64 GetComplexity() const {
            return CatFeatures.size() + std::min<ui64>(Splits.size(), 1);
        }

        SAVELOAD(Splits, CatFeatures);
        Y_SAVELOAD_DEFINE(Splits, CatFeatures);

    private:
        TVector<TBinarySplit> Splits;
        TVector<ui32> CatFeatures;
    };

    struct TCtr
    {
        TFeatureTensor FeatureTensor;
        TCtrConfig Configuration;

        TCtr(const TCtr& other) = default;

        TCtr() = default;

        TCtr(const TFeatureTensor& tensor,
             const TCtrConfig& config)
                : FeatureTensor(tensor)
                  , Configuration(config)
        {
        }

        bool operator==(const TCtr& other) const
        {
            return std::tie(FeatureTensor, Configuration) == std::tie(other.FeatureTensor, other.Configuration);
        }

        bool operator!=(const TCtr& other) const
        {
            return !(*this == other);
        }

        ui64 GetHash() const
        {
            return MultiHash(FeatureTensor, Configuration);
        }

        bool IsSimple() const
        {
            return FeatureTensor.IsSimple();
        }

        bool operator<(const TCtr& other) const
        {
            return std::tie(FeatureTensor, Configuration) < std::tie(other.FeatureTensor, other.Configuration);
        }

        SAVELOAD(FeatureTensor, Configuration);
        Y_SAVELOAD_DEFINE(FeatureTensor, Configuration);
    };

}

template<>
struct THash<NCatboostCuda::TBinarySplit>
{
    inline size_t operator()(const NCatboostCuda::TBinarySplit& value) const
    {
        return value.GetHash();
    }
};

template<>
struct THash<NCatboostCuda::TFeatureTensor>
{
    inline size_t operator()(const NCatboostCuda::TFeatureTensor& tensor) const
    {
        return tensor.GetHash();
    }
};

template<>
struct THash<NCatboostCuda::TCtrConfig>
{
    inline size_t operator()(const NCatboostCuda::TCtrConfig& config) const
    {
        return config.GetHash();
    }
};

template<>
struct THash<NCatboostCuda::TCtr>
{
    inline size_t operator()(const NCatboostCuda::TCtr& value) const
    {
        return value.GetHash();
    }
};