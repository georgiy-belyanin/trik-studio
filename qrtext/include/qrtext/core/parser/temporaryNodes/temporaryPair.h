#pragma once

#include "qrtext/core/ast/node.h"

namespace qrtext {
namespace core {

class TemporaryPair : public ast::Node {
public:
	TemporaryPair(QSharedPointer<ast::Node> left, QSharedPointer<ast::Node> right)
		: mLeft(left), mRight(right)
	{
	}

	QSharedPointer<ast::Node> left() const
	{
		return mLeft;
	}

	QSharedPointer<ast::Node> right() const
	{
		return mRight;
	}

private:
	QSharedPointer<ast::Node> mLeft;
	QSharedPointer<ast::Node> mRight;
};

}
}
