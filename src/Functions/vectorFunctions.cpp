#include <Columns/ColumnTuple.h>
#include <DataTypes/DataTypeTuple.h>
#include <DataTypes/DataTypesNumber.h>
#include <Functions/FunctionFactory.h>
#include <Functions/FunctionHelpers.h>
#include <Functions/TupleIFunction.h>
#include <Functions/castTypeToEither.h>

namespace DB
{
namespace ErrorCodes
{
    extern const int ILLEGAL_TYPE_OF_ARGUMENT;
}

template <const char * func_name>
class TuplesToTupleFunction : public TupleIFunction {
public:
    explicit TuplesToTupleFunction(ContextPtr context_) : TupleIFunction(context_) {}

    size_t getNumberOfArguments() const override { return 2; }

    DataTypePtr getReturnTypeImpl(const ColumnsWithTypeAndName & arguments) const override
    {
        const auto * left_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());
        const auto * right_tuple = checkAndGetDataType<DataTypeTuple>(arguments[1].type.get());

        if (!left_tuple)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument 0 of function {} should be tuples, got {}",
                            getName(), arguments[0].type->getName());

        if (!right_tuple)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument 1 of function {} should be tuples, got {}",
                            getName(), arguments[1].type->getName());

        const auto & left_types = left_tuple->getElements();
        const auto & right_types = right_tuple->getElements();

        Columns left_elements;
        Columns right_elements;
        if (arguments[0].column)
            left_elements = getTupleElements(*arguments[0].column);
        if (arguments[1].column)
            right_elements = getTupleElements(*arguments[1].column);

        if (left_types.size() != right_types.size())
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                            "Expected tuples of the same size as arguments of function {}. Got {} and {}",
                            getName(), arguments[0].type->getName(), arguments[1].type->getName());

        size_t tuple_size = left_types.size();
        if (tuple_size == 0)
            return std::make_shared<DataTypeUInt8>();

        auto func = FunctionFactory::instance().get(func_name, context);
        DataTypes types(tuple_size);
        for (size_t i = 0; i < tuple_size; ++i)
        {
            try
            {
                ColumnWithTypeAndName left{left_elements.empty() ? nullptr : left_elements[i], left_types[i], {}};
                ColumnWithTypeAndName right{right_elements.empty() ? nullptr : right_elements[i], right_types[i], {}};
                auto elem_func = func->build(ColumnsWithTypeAndName{left, right});
                types[i] = elem_func->getResultType();
            }
            catch (DB::Exception & e)
            {
                e.addMessage("While executing function {} for tuple element {}", getName(), i);
                throw;
            }
        }

        return std::make_shared<DataTypeTuple>(types);
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t input_rows_count) const override
    {
        const auto * left_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());
        const auto * right_tuple = checkAndGetDataType<DataTypeTuple>(arguments[1].type.get());
        const auto & left_types = left_tuple->getElements();
        const auto & right_types = right_tuple->getElements();
        auto left_elements = getTupleElements(*arguments[0].column);
        auto right_elements = getTupleElements(*arguments[1].column);

        size_t tuple_size = left_elements.size();
        if (tuple_size == 0)
            return DataTypeUInt8().createColumnConstWithDefaultValue(input_rows_count);

        auto func = FunctionFactory::instance().get(func_name, context);
        Columns columns(tuple_size);
        for (size_t i = 0; i < tuple_size; ++i)
        {
            ColumnWithTypeAndName left{left_elements[i], left_types[i], {}};
            ColumnWithTypeAndName right{right_elements[i], right_types[i], {}};
            auto elem_func = func->build(ColumnsWithTypeAndName{left, right});
            columns[i] = elem_func->execute({left, right}, elem_func->getResultType(), input_rows_count);
        }

        return ColumnTuple::create(columns);
    }
};

static const char PLUS_NAME[] = "plus";
class FunctionTuplePlus : public TuplesToTupleFunction<PLUS_NAME>
{
public:
    static constexpr auto name = "tuplePlus";

    explicit FunctionTuplePlus(ContextPtr context_) : TuplesToTupleFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionTuplePlus>(context_); }

    String getName() const override { return name; }
};

static const char MINUS_NAME[] = "minus";
class FunctionTupleMinus : public TuplesToTupleFunction<MINUS_NAME>
{
public:
    static constexpr auto name = "tupleMinus";

    explicit FunctionTupleMinus(ContextPtr context_) : TuplesToTupleFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionTupleMinus>(context_); }

    String getName() const override { return name; }
};

static const char MULTIPLY_NAME[] = "multiply";
class FunctionTupleMultiply : public TuplesToTupleFunction<MULTIPLY_NAME>
{
public:
    static constexpr auto name = "tupleMultiply";

    explicit FunctionTupleMultiply(ContextPtr context_) : TuplesToTupleFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionTupleMultiply>(context_); }

    String getName() const override { return name; }
};

static const char DIVIDE_NAME[] = "divide";
class FunctionTupleDivide : public TuplesToTupleFunction<DIVIDE_NAME>
{
public:
    static constexpr auto name = "tupleDivide";

    explicit FunctionTupleDivide(ContextPtr context_) : TuplesToTupleFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionTupleDivide>(context_); }

    String getName() const override { return name; }
};

class FunctionTupleNegate : public TupleIFunction
{
public:
    static constexpr auto name = "tupleNegate";

    explicit FunctionTupleNegate(ContextPtr context_) : TupleIFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionTupleNegate>(context_); }

    String getName() const override { return name; }

    size_t getNumberOfArguments() const override { return 1; }

    DataTypePtr getReturnTypeImpl(const ColumnsWithTypeAndName & arguments) const override
    {
        const auto * cur_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());

        if (!cur_tuple)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument 0 of function {} should be tuples, got {}",
                            getName(), arguments[0].type->getName());

        const auto & cur_types = cur_tuple->getElements();

        Columns cur_elements;
        if (arguments[0].column)
            cur_elements = getTupleElements(*arguments[0].column);

        size_t tuple_size = cur_types.size();
        if (tuple_size == 0)
            return std::make_shared<DataTypeUInt8>();

        auto negate = FunctionFactory::instance().get("negate", context);
        DataTypes types(tuple_size);
        for (size_t i = 0; i < tuple_size; ++i)
        {
            try
            {
                ColumnWithTypeAndName cur{cur_elements.empty() ? nullptr : cur_elements[i], cur_types[i], {}};
                auto elem_negate = negate->build(ColumnsWithTypeAndName{cur});
                types[i] = elem_negate->getResultType();
            }
            catch (DB::Exception & e)
            {
                e.addMessage("While executing function {} for tuple element {}", getName(), i);
                throw;
            }
        }

        return std::make_shared<DataTypeTuple>(types);
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t input_rows_count) const override
    {
        const auto * cur_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());
        const auto & cur_types = cur_tuple->getElements();
        auto cur_elements = getTupleElements(*arguments[0].column);

        size_t tuple_size = cur_elements.size();
        if (tuple_size == 0)
            return DataTypeUInt8().createColumnConstWithDefaultValue(input_rows_count);

        auto negate = FunctionFactory::instance().get("negate", context);
        Columns columns(tuple_size);
        for (size_t i = 0; i < tuple_size; ++i)
        {
            ColumnWithTypeAndName cur{cur_elements[i], cur_types[i], {}};
            auto elem_negate= negate->build(ColumnsWithTypeAndName{cur});
            columns[i] = elem_negate->execute({cur}, elem_negate->getResultType(), input_rows_count);
        }

        return ColumnTuple::create(columns);
    }
};

class FunctionDotProduct : public TupleIFunction
{
public:
    static constexpr auto name = "dotProduct";

    explicit FunctionDotProduct(ContextPtr context_) : TupleIFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionDotProduct>(context_); }

    String getName() const override { return name; }

    size_t getNumberOfArguments() const override { return 2; }

    DataTypePtr getReturnTypeImpl(const ColumnsWithTypeAndName & arguments) const override
    {
        const auto * left_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());
        const auto * right_tuple = checkAndGetDataType<DataTypeTuple>(arguments[1].type.get());

        if (!left_tuple)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument 0 of function {} should be tuples, got {}",
                            getName(), arguments[0].type->getName());

        if (!right_tuple)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument 1 of function {} should be tuples, got {}",
                            getName(), arguments[1].type->getName());

        const auto & left_types = left_tuple->getElements();
        const auto & right_types = right_tuple->getElements();

        Columns left_elements;
        Columns right_elements;
        if (arguments[0].column)
            left_elements = getTupleElements(*arguments[0].column);
        if (arguments[1].column)
            right_elements = getTupleElements(*arguments[1].column);

        if (left_types.size() != right_types.size())
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT,
                            "Expected tuples of the same size as arguments of function {}. Got {} and {}",
                            getName(), arguments[0].type->getName(), arguments[1].type->getName());

        size_t tuple_size = left_types.size();
        if (tuple_size == 0)
            return std::make_shared<DataTypeUInt8>();

        auto multiply = FunctionFactory::instance().get("multiply", context);
        auto plus = FunctionFactory::instance().get("plus", context);
        DataTypePtr res_type;
        for (size_t i = 0; i < tuple_size; ++i)
        {
            try
            {
                ColumnWithTypeAndName left{left_elements.empty() ? nullptr : left_elements[i], left_types[i], {}};
                ColumnWithTypeAndName right{right_elements.empty() ? nullptr : right_elements[i], right_types[i], {}};
                auto elem_multiply = multiply->build(ColumnsWithTypeAndName{left, right});

                if (i == 0)
                {
                    res_type = elem_multiply->getResultType();
                    continue;
                }

                ColumnWithTypeAndName left_type{res_type, {}};
                ColumnWithTypeAndName right_type{elem_multiply->getResultType(), {}};
                auto plus_elem = plus->build({left_type, right_type});
                res_type = plus_elem->getResultType();
            }
            catch (DB::Exception & e)
            {
                e.addMessage("While executing function {} for tuple element {}", getName(), i);
                throw;
            }
        }

        return res_type;
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t input_rows_count) const override
    {
        const auto * left_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());
        const auto * right_tuple = checkAndGetDataType<DataTypeTuple>(arguments[1].type.get());
        const auto & left_types = left_tuple->getElements();
        const auto & right_types = right_tuple->getElements();
        auto left_elements = getTupleElements(*arguments[0].column);
        auto right_elements = getTupleElements(*arguments[1].column);

        size_t tuple_size = left_elements.size();
        if (tuple_size == 0)
            return DataTypeUInt8().createColumnConstWithDefaultValue(input_rows_count);

        auto multiply = FunctionFactory::instance().get("multiply", context);
        auto plus = FunctionFactory::instance().get("plus", context);
        ColumnWithTypeAndName res;
        for (size_t i = 0; i < tuple_size; ++i)
        {
            ColumnWithTypeAndName left{left_elements[i], left_types[i], {}};
            ColumnWithTypeAndName right{right_elements[i], right_types[i], {}};
            auto elem_multiply = multiply->build(ColumnsWithTypeAndName{left, right});

            ColumnWithTypeAndName column;
            column.type = elem_multiply->getResultType();
            column.column = elem_multiply->execute({left, right}, column.type, input_rows_count);

            if (i == 0)
            {
                res = std::move(column);
            }
            else
            {
                auto plus_elem = plus->build({res, column});
                auto res_type = plus_elem->getResultType();
                res.column = plus_elem->execute({res, column}, res_type, input_rows_count);
                res.type = res_type;
            }
        }

        return res.column;
    }
};

class FunctionL1Norm : public TupleIFunction
{
public:
    static constexpr auto name = "L1Norm";

    explicit FunctionL1Norm(ContextPtr context_) : TupleIFunction(context_) {}
    static FunctionPtr create(ContextPtr context_) { return std::make_shared<FunctionL1Norm>(context_); }

    String getName() const override { return name; }

    size_t getNumberOfArguments() const override { return 1; }

    DataTypePtr getReturnTypeImpl(const ColumnsWithTypeAndName & arguments) const override
    {
        const auto * cur_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());

        if (!cur_tuple)
            throw Exception(ErrorCodes::ILLEGAL_TYPE_OF_ARGUMENT, "Argument 0 of function {} should be tuples, got {}",
                            getName(), arguments[0].type->getName());

        const auto & cur_types = cur_tuple->getElements();

        Columns cur_elements;
        if (arguments[0].column)
            cur_elements = getTupleElements(*arguments[0].column);

        size_t tuple_size = cur_types.size();
        if (tuple_size == 0)
            return std::make_shared<DataTypeUInt8>();

        auto abs = FunctionFactory::instance().get("abs", context);
        auto plus = FunctionFactory::instance().get("plus", context);
        DataTypePtr res_type;
        for (size_t i = 0; i < tuple_size; ++i)
        {
            try
            {
                ColumnWithTypeAndName cur{cur_elements.empty() ? nullptr : cur_elements[i], cur_types[i], {}};
                auto elem_abs = abs->build(ColumnsWithTypeAndName{cur});

                if (i == 0)
                {
                    res_type = elem_abs->getResultType();
                    continue;
                }

                ColumnWithTypeAndName left_type{res_type, {}};
                ColumnWithTypeAndName right_type{elem_abs->getResultType(), {}};
                auto plus_elem = plus->build({left_type, right_type});
                res_type = plus_elem->getResultType();
            }
            catch (DB::Exception & e)
            {
                e.addMessage("While executing function {} for tuple element {}", getName(), i);
                throw;
            }
        }

        return res_type;
    }

    ColumnPtr executeImpl(const ColumnsWithTypeAndName & arguments, const DataTypePtr &, size_t input_rows_count) const override
    {
        const auto * cur_tuple = checkAndGetDataType<DataTypeTuple>(arguments[0].type.get());
        const auto & cur_types = cur_tuple->getElements();
        auto cur_elements = getTupleElements(*arguments[0].column);

        size_t tuple_size = cur_elements.size();
        if (tuple_size == 0)
            return DataTypeUInt8().createColumnConstWithDefaultValue(input_rows_count);

        auto abs = FunctionFactory::instance().get("abs", context);
        auto plus = FunctionFactory::instance().get("plus", context);
        ColumnWithTypeAndName res;
        for (size_t i = 0; i < tuple_size; ++i)
        {
            ColumnWithTypeAndName cur{cur_elements[i], cur_types[i], {}};
            auto elem_abs = abs->build(ColumnsWithTypeAndName{cur});

            ColumnWithTypeAndName column;
            column.type = elem_abs->getResultType();
            column.column = elem_abs->execute({cur}, column.type, input_rows_count);

            if (i == 0)
            {
                res = std::move(column);
            }
            else
            {
                auto plus_elem = plus->build({res, column});
                auto res_type = plus_elem->getResultType();
                res.column = plus_elem->execute({res, column}, res_type, input_rows_count);
                res.type = res_type;
            }
        }

        return res.column;
    }
};

void registerVectorFunctions(FunctionFactory & factory)
{
    factory.registerFunction<FunctionTuplePlus>();
    factory.registerAlias("vectorSum", FunctionTuplePlus::name, FunctionFactory::CaseInsensitive);
    factory.registerFunction<FunctionTupleMinus>();
    factory.registerAlias("vectorDifference", FunctionTupleMinus::name, FunctionFactory::CaseInsensitive);
    factory.registerFunction<FunctionTupleMultiply>();
    factory.registerFunction<FunctionTupleDivide>();
    factory.registerFunction<FunctionTupleNegate>();

    //factory.registerFunction<FunctionTupleMultiplyByNumber>();
    //factory.registerFunction<FunctionTupleDivideByNumber>();

    factory.registerFunction<FunctionDotProduct>();
    factory.registerAlias("scalarProduct", FunctionDotProduct::name, FunctionFactory::CaseInsensitive);

    factory.registerFunction<FunctionL1Norm>();
    /*factory.registerFunction<FunctionL1Distance>();
    factory.registerFunction<FunctionL1Normalize>();

    factory.registerFunction<FunctionL2Norm>();
    factory.registerFunction<FunctionL2Distance>();
    factory.registerFunction<FunctionL2Normalize>();

    factory.registerFunction<FunctionLinfNorm>();
    factory.registerFunction<FunctionLinfDistance>();
    factory.registerFunction<FunctionLinfNormalize>();

    factory.registerFunction<FunctionLpNorm>();
    factory.registerFunction<FunctionLpDistance>();
    factory.registerFunction<FunctionLpNormalize>();

    factory.registerFunction<FunctionCosineDistance>();*/
}
}
