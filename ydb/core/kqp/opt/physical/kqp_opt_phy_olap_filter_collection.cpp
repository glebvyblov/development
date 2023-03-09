#include "kqp_opt_phy_olap_filter_collection.h"

#include <ydb/library/yql/core/yql_expr_type_annotation.h>
#include <ydb/library/yql/utils/log/log.h>

#include <vector>

namespace NKikimr::NKqp::NOpt {

using namespace NYql;
using namespace NYql::NNodes;

namespace {

bool IsSupportedPredicate(const TCoCompare& predicate) {
    if (predicate.Maybe<TCoCmpEqual>()) {
        return true;
    }

    if (predicate.Maybe<TCoCmpLess>()) {
        return true;
    }

    if (predicate.Maybe<TCoCmpGreater>()) {
        return true;
    }

    if (predicate.Maybe<TCoCmpNotEqual>()) {
        return true;
    }

    if (predicate.Maybe<TCoCmpGreaterOrEqual>()) {
        return true;
    }

    if (predicate.Maybe<TCoCmpLessOrEqual>()) {
        return true;
    }

    return false;
}

bool IsSupportedDataType(const TCoDataCtor& node) {
    if (node.Maybe<TCoUtf8>() ||
        node.Maybe<TCoString>() ||
        node.Maybe<TCoBool>() ||
        node.Maybe<TCoFloat>() ||
        node.Maybe<TCoDouble>() ||
        node.Maybe<TCoInt8>() ||
        node.Maybe<TCoInt16>() ||
        node.Maybe<TCoInt32>() ||
        node.Maybe<TCoInt64>() ||
        node.Maybe<TCoUint8>() ||
        node.Maybe<TCoUint16>() ||
        node.Maybe<TCoUint32>() ||
        node.Maybe<TCoUint64>())
    {
        return true;
    }

    return false;
}

bool IsSupportedCast(const TCoSafeCast& cast) {
    auto maybeDataType = cast.Type().Maybe<TCoDataType>();
    if (!maybeDataType) {
        if (const auto maybeOptionalType = cast.Type().Maybe<TCoOptionalType>()) {
            maybeDataType = maybeOptionalType.Cast().ItemType().Maybe<TCoDataType>();
        }
    }
    YQL_ENSURE(maybeDataType.IsValid());

    auto dataType = maybeDataType.Cast();
    if (dataType.Type().Value() == "Int32") {
        return cast.Value().Maybe<TCoString>().IsValid();
    } else if (dataType.Type().Value() == "Timestamp") {
        return cast.Value().Maybe<TCoUint32>().IsValid();
    }
    return false;
}

bool IsComparableTypes(const TExprBase& leftNode, const TExprBase& rightNode, bool equality,
    const TTypeAnnotationNode* inputType)
{
    const TExprNode::TPtr leftPtr = leftNode.Ptr();
    const TExprNode::TPtr rightPtr = rightNode.Ptr();

    auto getDataType = [inputType](const TExprNode::TPtr& node) {
        auto type = node->GetTypeAnn();

        if (type->GetKind() == ETypeAnnotationKind::Unit) {
            auto rowType = inputType->Cast<TStructExprType>();
            type = rowType->FindItemType(node->Content());
        }

        if (type->GetKind() == ETypeAnnotationKind::Optional) {
            type = type->Cast<TOptionalExprType>()->GetItemType();
        }

        return type;
    };

    auto defaultCompare = [equality](const TTypeAnnotationNode* left, const TTypeAnnotationNode* right) {
        if (equality) {
            return CanCompare<true>(left, right);
        }

        return CanCompare<false>(left, right);
    };

    auto canCompare = [&defaultCompare](const TTypeAnnotationNode* left, const TTypeAnnotationNode* right) {
        if (left->GetKind() != ETypeAnnotationKind::Data ||
            right->GetKind() != ETypeAnnotationKind::Data)
        {
            return defaultCompare(left, right);
        }

        auto leftTypeId = GetDataTypeInfo(left->Cast<TDataExprType>()->GetSlot()).TypeId;
        auto rightTypeId = GetDataTypeInfo(right->Cast<TDataExprType>()->GetSlot()).TypeId;

        if (leftTypeId == rightTypeId) {
            return ECompareOptions::Comparable;
        }

        /*
         * Check special case UInt32 <-> Datetime in case i can't put it inside switch without lot of copypaste
         */
        if (leftTypeId == NYql::NProto::Uint32 && rightTypeId == NYql::NProto::Date) {
            return ECompareOptions::Comparable;
        }

        /*
         * SSA program requires strict equality of some types, otherwise columnshard fails to execute comparison
         */
        switch (leftTypeId) {
            case NYql::NProto::Int8:
            case NYql::NProto::Int16:
            case NYql::NProto::Int32:
                // SSA program cast those values to Int32
                if (rightTypeId == NYql::NProto::Int8 ||
                    rightTypeId == NYql::NProto::Int16 ||
                    rightTypeId == NYql::NProto::Int32)
                {
                    return ECompareOptions::Comparable;
                }
                break;
            case NYql::NProto::Uint16:
                if (rightTypeId == NYql::NProto::Date) {
                    return ECompareOptions::Comparable;
                }
                [[fallthrough]];
            case NYql::NProto::Uint8:
            case NYql::NProto::Uint32:
                // SSA program cast those values to Uint32
                if (rightTypeId == NYql::NProto::Uint8 ||
                    rightTypeId == NYql::NProto::Uint16 ||
                    rightTypeId == NYql::NProto::Uint32)
                {
                    return ECompareOptions::Comparable;
                }
                break;
            case NYql::NProto::Date:
                // See arcadia/ydb/library/yql/dq/runtime/dq_arrow_helpers.cpp SwitchMiniKQLDataTypeToArrowType
                if (rightTypeId == NYql::NProto::Uint16) {
                    return ECompareOptions::Comparable;
                }
                break;
            case NYql::NProto::Datetime:
                // See arcadia/ydb/library/yql/dq/runtime/dq_arrow_helpers.cpp SwitchMiniKQLDataTypeToArrowType
                if (rightTypeId == NYql::NProto::Uint32) {
                    return ECompareOptions::Comparable;
                }
                break;
            case NYql::NProto::Bool:
            case NYql::NProto::Int64:
            case NYql::NProto::Uint64:
            case NYql::NProto::Float:
            case NYql::NProto::Double:
            case NYql::NProto::Decimal:
            case NYql::NProto::Timestamp:
            case NYql::NProto::Interval:
                // Obviosly here right node has not same type as left one
                break;
            default:
                return defaultCompare(left, right);
        }

        return ECompareOptions::Uncomparable;
    };

    auto leftType = getDataType(leftPtr);
    auto rightType = getDataType(rightPtr);

    if (canCompare(leftType, rightType) == ECompareOptions::Uncomparable) {
        YQL_CLOG(DEBUG, ProviderKqp) << "OLAP Pushdown: "
            << "Uncompatible types in compare of nodes: "
            << leftPtr->Content() << " of type " << FormatType(leftType)
            << " and "
            << rightPtr->Content() << " of type " << FormatType(rightType);

        return false;
    }

    return true;
}

std::vector<TExprBase> GetComparisonNodes(const TExprBase& node) {
    std::vector<TExprBase> res;
    if (node.Maybe<TExprList>()) {
        auto nodeList = node.Cast<TExprList>();
        res.reserve(nodeList.Size());
        for (size_t i = 0; i < nodeList.Size(); ++i) {
            res.emplace_back(nodeList.Item(i));
        }
    } else {
        res.emplace_back(node);
    }
    return res;
}

bool CheckComparisonNodeForPushdown(const TExprBase& node, const TExprNode* lambdaArg) {
    if (auto maybeSafeCast = node.Maybe<TCoSafeCast>()) {
        if (!IsSupportedCast(maybeSafeCast.Cast())) {
            return false;
        }
    } else if (auto maybeData = node.Maybe<TCoDataCtor>()) {
        if (!IsSupportedDataType(maybeData.Cast())) {
            return false;
        }
    } else if (auto maybeMember = node.Maybe<TCoMember>()) {
        if (maybeMember.Cast().Struct().Raw() != lambdaArg) {
            return false;
        }
    } else if (!node.Maybe<TCoNull>() && !node.Maybe<TCoParameter>()) {
        return false;
    }

    return true;
}

bool CheckComparisonParametersForPushdown(const TCoCompare& compare, const TExprNode* lambdaArg, const TExprBase& input) {
    const TTypeAnnotationNode* inputType = input.Ptr()->GetTypeAnn();
    switch (inputType->GetKind()) {
        case ETypeAnnotationKind::Flow:
            inputType = inputType->Cast<TFlowExprType>()->GetItemType();
            break;
        case ETypeAnnotationKind::Stream:
            inputType = inputType->Cast<TStreamExprType>()->GetItemType();
            break;
        default:
            YQL_ENSURE(false, "Unsupported type of incoming data: " << (ui32)inputType->GetKind());
            // We do not know how process input that is not a sequence of elements
            return false;
    }
    YQL_ENSURE(inputType->GetKind() == ETypeAnnotationKind::Struct);

    if (inputType->GetKind() != ETypeAnnotationKind::Struct) {
        // We do not know how process input that is not a sequence of elements
        return false;
    }

    bool equality = compare.Maybe<TCoCmpEqual>() || compare.Maybe<TCoCmpNotEqual>();
    auto leftList = GetComparisonNodes(compare.Left());
    auto rightList = GetComparisonNodes(compare.Right());
    YQL_ENSURE(leftList.size() == rightList.size(), "Different sizes of lists in comparison!");

    for (size_t i = 0; i < leftList.size(); ++i) {
        if (!CheckComparisonNodeForPushdown(leftList[i], lambdaArg) || !CheckComparisonNodeForPushdown(rightList[i], lambdaArg)) {
            return false;
        }
        if (!IsComparableTypes(leftList[i], rightList[i], equality, inputType)) {
            return false;
        }
    }

    return true;
}

bool CompareCanBePushed(const TCoCompare& compare, const TExprNode* lambdaArg, const TExprBase& lambdaBody) {
    if (!IsSupportedPredicate(compare)) {
        return false;
    }

    if (!CheckComparisonParametersForPushdown(compare, lambdaArg, lambdaBody)) {
        return false;
    }

    return true;
}

bool SafeCastCanBePushed(const TCoFlatMap& flatmap, const TExprNode* lambdaArg) {
    /*
     * There are three ways of comparison in following format:
     *
     * FlatMap (LeftArgument, FlatMap(RightArgument(), Just(Predicate))
     *
     * Examples:
     * FlatMap (SafeCast(), FlatMap(Member(), Just(Comparison))
     * FlatMap (Member(), FlatMap(SafeCast(), Just(Comparison))
     * FlatMap (SafeCast(), FlatMap(SafeCast(), Just(Comparison))
     */
    auto maybeFlatmap = flatmap.Lambda().Body().Maybe<TCoFlatMap>();
    if (!maybeFlatmap.IsValid()) {
        return false;
    }

    auto leftList = GetComparisonNodes(flatmap.Input());
    auto rightList = GetComparisonNodes(maybeFlatmap.Cast().Input());
    YQL_ENSURE(leftList.size() == rightList.size(), "Different sizes of lists in comparison!");

    for (size_t i = 0; i < leftList.size(); ++i) {
        if (!CheckComparisonNodeForPushdown(leftList[i], lambdaArg) || !CheckComparisonNodeForPushdown(rightList[i], lambdaArg)) {
            return false;
        }
    }

    auto maybeJust = maybeFlatmap.Cast().Lambda().Body().Maybe<TCoJust>();
    if (!maybeJust.IsValid()) {
        return false;
    }

    auto maybePredicate = maybeJust.Cast().Input().Maybe<TCoCompare>();
    if (!maybePredicate.IsValid()) {
        return false;
    }

    auto predicate = maybePredicate.Cast();
    if (!IsSupportedPredicate(predicate)) {
        return false;
    }

    return true;
}

bool CoalesceCanBePushed(const TCoCoalesce& coalesce, const TExprNode* lambdaArg, const TExprBase& lambdaBody) {
    if (!coalesce.Value().Maybe<TCoBool>()) {
        return false;
    }

    if (coalesce.Value().Cast<TCoBool>().Literal().Value() != "false") {
        // Maybe we don't need this check
        return false;
    }

    if (auto maybeCompare = coalesce.Predicate().Maybe<TCoCompare>()) {
        return CompareCanBePushed(maybeCompare.Cast(), lambdaArg, lambdaBody);
    } else if (auto maybeFlatmap = coalesce.Predicate().Maybe<TCoFlatMap>()) {
        return SafeCastCanBePushed(maybeFlatmap.Cast(), lambdaArg);
    }

    return false;
}

bool ExistsCanBePushed(const TCoExists& exists, const TExprNode* lambdaArg) {
    auto maybeMember = exists.Optional().Maybe<TCoMember>();
    if (!maybeMember.IsValid()) {
        return false;
    }
    if (maybeMember.Cast().Struct().Raw() != lambdaArg) {
        return false;
    }
    return true;
}

void CollectPredicatesForBinaryBoolOperators(const TExprBase& opNode, TPredicateNode& predicateTree, const TExprNode* lambdaArg, const TExprBase& lambdaBody) {
    if (!opNode.Maybe<TCoAnd>() && !opNode.Maybe<TCoOr>() && !opNode.Maybe<TCoOr>()) {
        return;
    }
    predicateTree.Children.reserve(opNode.Ptr()->ChildrenSize());
    predicateTree.CanBePushed = true;
    for (auto& childNodePtr: opNode.Ptr()->Children()) {
        TPredicateNode child(childNodePtr);
        CollectPredicates(TExprBase(childNodePtr), child, lambdaArg, lambdaBody);
        predicateTree.Children.emplace_back(child);
        predicateTree.CanBePushed &= child.CanBePushed;
    }
}

} // anonymous namespace end

void CollectPredicates(const TExprBase& predicate, TPredicateNode& predicateTree, const TExprNode* lambdaArg, const TExprBase& lambdaBody) {
    if (predicate.Maybe<TCoCoalesce>()) {
        auto coalesce = predicate.Cast<TCoCoalesce>();
        predicateTree.CanBePushed = CoalesceCanBePushed(coalesce, lambdaArg, lambdaBody);
    } else if (predicate.Maybe<TCoCompare>()) {
        auto compare = predicate.Cast<TCoCompare>();
        predicateTree.CanBePushed = CompareCanBePushed(compare, lambdaArg, lambdaBody);
    } else if (predicate.Maybe<TCoExists>()) {
        auto exists = predicate.Cast<TCoExists>();
        predicateTree.CanBePushed = ExistsCanBePushed(exists, lambdaArg);
    } else if (predicate.Maybe<TCoNot>()) {
        predicateTree.Op = EBoolOp::Not;
        auto notOp = predicate.Cast<TCoNot>();
        TPredicateNode child(notOp.Value());
        CollectPredicates(notOp.Value(), child, lambdaArg, lambdaBody);
        predicateTree.CanBePushed = child.CanBePushed;
        predicateTree.Children.emplace_back(child);
    } else if (predicate.Maybe<TCoAnd>()) {
        predicateTree.Op = EBoolOp::And;
        CollectPredicatesForBinaryBoolOperators(predicate.Cast<TCoAnd>(), predicateTree, lambdaArg, lambdaBody);
    } else if (predicate.Maybe<TCoOr>()) {
        predicateTree.Op = EBoolOp::Or;
        CollectPredicatesForBinaryBoolOperators(predicate.Cast<TCoOr>(), predicateTree, lambdaArg, lambdaBody);
    } else if (predicate.Maybe<TCoXor>()) {
        predicateTree.Op = EBoolOp::Xor;
        CollectPredicatesForBinaryBoolOperators(predicate.Cast<TCoXor>(), predicateTree, lambdaArg, lambdaBody);
    } else {
        predicateTree.CanBePushed = false;
    }
}

} // namespace NKikimr::NKqp::NOpt