#include "ConnectionGraphicsObject.hpp"

#include <QtWidgets/QGraphicsSceneMouseEvent>
#include <QtWidgets/QGraphicsDropShadowEffect>
#include <QtWidgets/QGraphicsBlurEffect>
#include <QtWidgets/QStyleOptionGraphicsItem>
#include <QtWidgets/QGraphicsView>

#include "FlowScene.hpp"

#include "Connection.hpp"
#include "ConnectionGeometry.hpp"
#include "ConnectionPainter.hpp"
#include "ConnectionState.hpp"
#include "ConnectionBlurEffect.hpp"

#include "NodeGraphicsObject.hpp"

#include "NodeConnectionInteraction.hpp"

#include "Node.hpp"

#include <QDebug>

using QtNodes::ConnectionGraphicsObject;
using QtNodes::Connection;
using QtNodes::FlowScene;

ConnectionGraphicsObject::
ConnectionGraphicsObject(FlowScene &scene,
                         Connection &connection)
  : _scene(scene)
  , _connection(connection)
{
  _scene.addItem(this);

  setFlag(QGraphicsItem::ItemIsMovable, true);
  setFlag(QGraphicsItem::ItemIsFocusable, true);
  setFlag(QGraphicsItem::ItemIsSelectable, true);

  setAcceptHoverEvents(true);

  // addGraphicsEffect();

  setZValue(-1.0);
}


ConnectionGraphicsObject::
~ConnectionGraphicsObject()
{
  std::cout << "Remove ConnectionGraphicsObject from scene" << std::endl;

  _scene.removeItem(this);
}


QtNodes::Connection&
ConnectionGraphicsObject::
connection()
{
  return _connection;
}


QRectF
ConnectionGraphicsObject::
boundingRect() const
{
  return _connection.connectionGeometry().boundingRect();
}


QPainterPath
ConnectionGraphicsObject::
shape() const
{
#ifdef DEBUG_DRAWING

  //QPainterPath path;

  //path.addRect(boundingRect());
  //return path;

#else
  auto const &geom =
    _connection.connectionGeometry();

  return ConnectionPainter::getPainterStroke(geom);

#endif
}


void
ConnectionGraphicsObject::
setGeometryChanged()
{
  prepareGeometryChange();
}


void
ConnectionGraphicsObject::
move()
{

  auto moveEndPoint =
  [this] (PortType portType)
  {
    if (auto node = _connection.getNode(portType))
    {
      auto const &nodeGraphics = node->nodeGraphicsObject();

      auto const &nodeGeom = node->nodeGeometry();

      QPointF scenePos =
        nodeGeom.portScenePosition(_connection.getPortIndex(portType),
                                   portType,
                                   nodeGraphics.sceneTransform());

      {
        QTransform sceneTransform = this->sceneTransform();

        QPointF connectionPos = sceneTransform.inverted().map(scenePos);

        _connection.connectionGeometry().setEndPoint(portType,
                                                     connectionPos);

        _connection.getConnectionGraphicsObject().setGeometryChanged();
        _connection.getConnectionGraphicsObject().update();
      }
    }
  };

  moveEndPoint(PortType::In);
  moveEndPoint(PortType::Out);
}

void ConnectionGraphicsObject::lock(bool locked)
{
  setFlag(QGraphicsItem::ItemIsMovable, !locked);
  setFlag(QGraphicsItem::ItemIsFocusable, !locked);
  setFlag(QGraphicsItem::ItemIsSelectable, !locked);
}


void
ConnectionGraphicsObject::
paint(QPainter* painter,
      QStyleOptionGraphicsItem const* option,
      QWidget*)
{
  painter->setClipRect(option->exposedRect);

  ConnectionPainter::paint(painter,
                           _connection);
}


void
ConnectionGraphicsObject::
mousePressEvent(QGraphicsSceneMouseEvent* event)
{
  QGraphicsItem::mousePressEvent(event);
  //event->ignore();
}


void
ConnectionGraphicsObject::
mouseMoveEvent(QGraphicsSceneMouseEvent* event)
{
  prepareGeometryChange();

  auto view = static_cast<QGraphicsView*>(event->widget());
  auto node = locateNodeAt(event->scenePos(),
                           _scene,
                           view->transform());

  auto &state = _connection.connectionState();

  state.interactWithNode(node);
  if (node)
  {
    node->reactToPossibleConnection(state.requiredPort(),
                                    _connection.dataType(),
                                    event->scenePos());
  }

  //-------------------

  QPointF offset = event->pos() - event->lastPos();

  auto requiredPort = _connection.requiredPort();

  if (requiredPort != PortType::None)
  {
    _connection.connectionGeometry().moveEndPoint(requiredPort, offset);
  }

  //-------------------

  update();

  event->accept();
}


void
ConnectionGraphicsObject::
mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
{
  ungrabMouse();
  event->accept();

  auto node = locateNodeAt(event->scenePos(), _scene,
                           _scene.views()[0]->transform());

  NodeConnectionInteraction interaction(*node, _connection, _scene);

  bool altag = false;
#ifdef ALTAG
  altag=true;
#endif

  if (altag && !node && _connection.requiredPort() == PortType::Out)
  {
    QString modelName = _connection.dataType().id;
    qInfo() << modelName;

    auto type = _scene.registry().create(modelName);

    if (type)
    {
      qInfo() << "in";
      auto nodeCrea = &(_scene.createNode(std::move(type)));
      qInfo() << "node created";

      //We need to put the node right under the compatible port
      NodeDataModel *ndm = nodeCrea->nodeDataModel();
      int nbp = ndm->nPorts(PortType::Out);
      int pid = 0;
      for(int i=0;i<nbp;i++)
      {
        if (ndm->dataType(PortType::Out,i).id == _connection.dataType().id)
          pid = i;
      }

      QPointF pos = nodeCrea->nodeGeometry().portScenePosition(pid,PortType::Out);
      qInfo() << pos;
      QPointF evPos = event->scenePos();
      QPointF newPos = QPointF(evPos.x()-pos.x(),evPos.y()-pos.y());
      nodeCrea->nodeGraphicsObject().setPos(newPos);

      qInfo() << "node set pos";

      NodeConnectionInteraction interactionCrea(*nodeCrea, _connection, _scene);
      qInfo() << "interaction created";
      qInfo() << nodeCrea << interactionCrea.tryConnect();
      /*
      if (interactionCrea.tryConnect())
      {
        qInfo() << "tryConnect ok";
        nodeCrea->resetReactionToConnection();
        qInfo() << "Created!";
      }
      */
    }
    else
    {
      _scene.deleteConnection(_connection);
    }
  }
  else if (node && interaction.tryConnect())
  {
    node->resetReactionToConnection();
    qInfo() << "Created!";
  }
  else if (_connection.connectionState().requiresPort())
  {
    _scene.deleteConnection(_connection);
  }
}


void
ConnectionGraphicsObject::
hoverEnterEvent(QGraphicsSceneHoverEvent* event)
{
  _connection.connectionGeometry().setHovered(true);

  update();
  _scene.connectionHovered(connection(), event->screenPos());
  event->accept();
}


void
ConnectionGraphicsObject::
hoverLeaveEvent(QGraphicsSceneHoverEvent* event)
{
  _connection.connectionGeometry().setHovered(false);

  update();
  _scene.connectionHoverLeft(connection());
  event->accept();
}


void
ConnectionGraphicsObject::
addGraphicsEffect()
{
  auto effect = new QGraphicsBlurEffect;

  effect->setBlurRadius(5);
  setGraphicsEffect(effect);

  //auto effect = new QGraphicsDropShadowEffect;
  //auto effect = new ConnectionBlurEffect(this);
  //effect->setOffset(4, 4);
  //effect->setColor(QColor(Qt::gray).darker(800));
}
