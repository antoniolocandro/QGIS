/***************************************************************************
  qgslayertreegroup.cpp
  --------------------------------------
  Date                 : May 2014
  Copyright            : (C) 2014 by Martin Dobias
  Email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgslayertreegroup.h"

#include "qgslayertree.h"
#include "qgslayertreeutils.h"

#include <QDomElement>
#include <QStringList>


QgsLayerTreeGroup::QgsLayerTreeGroup( const QString& name, Qt::CheckState checked )
    : QgsLayerTreeNode( NodeGroup )
    , mName( name )
    , mChecked( checked )
    , mChangingChildVisibility( false )
{
  connect( this, SIGNAL( visibilityChanged( QgsLayerTreeNode*, Qt::CheckState ) ), this, SLOT( nodeVisibilityChanged( QgsLayerTreeNode* ) ) );
}

QgsLayerTreeGroup::QgsLayerTreeGroup( const QgsLayerTreeGroup& other )
    : QgsLayerTreeNode( other )
    , mName( other.mName )
    , mChecked( other.mChecked )
    , mChangingChildVisibility( false )
{
  connect( this, SIGNAL( visibilityChanged( QgsLayerTreeNode*, Qt::CheckState ) ), this, SLOT( nodeVisibilityChanged( QgsLayerTreeNode* ) ) );
}


QgsLayerTreeGroup* QgsLayerTreeGroup::addGroup( const QString &name )
{
  QgsLayerTreeGroup* grp = new QgsLayerTreeGroup( name );
  addChildNode( grp );
  return grp;
}

QgsLayerTreeLayer*QgsLayerTreeGroup::insertLayer( int index, QgsMapLayer* layer )
{
  QgsLayerTreeLayer* ll = new QgsLayerTreeLayer( layer );
  insertChildNode( index, ll );
  return ll;
}

QgsLayerTreeLayer* QgsLayerTreeGroup::addLayer( QgsMapLayer* layer )
{
  QgsLayerTreeLayer* ll = new QgsLayerTreeLayer( layer );
  addChildNode( ll );
  return ll;
}

void QgsLayerTreeGroup::insertChildNode( int index, QgsLayerTreeNode* node )
{
  QList<QgsLayerTreeNode*> nodes;
  nodes << node;
  insertChildNodes( index, nodes );
}

void QgsLayerTreeGroup::insertChildNodes( int index, QList<QgsLayerTreeNode*> nodes )
{
  // low-level insert
  insertChildren( index, nodes );

  updateVisibilityFromChildren();
}

void QgsLayerTreeGroup::addChildNode( QgsLayerTreeNode* node )
{
  insertChildNode( -1, node );
}

void QgsLayerTreeGroup::removeChildNode( QgsLayerTreeNode *node )
{
  int i = mChildren.indexOf( node );
  if ( i >= 0 )
    removeChildAt( i );
}

void QgsLayerTreeGroup::removeLayer( QgsMapLayer* layer )
{
  foreach ( QgsLayerTreeNode* child, mChildren )
  {
    if ( QgsLayerTree::isLayer( child ) )
    {
      QgsLayerTreeLayer* childLayer = QgsLayerTree::toLayer( child );
      if ( childLayer->layer() == layer )
      {
        removeChildAt( mChildren.indexOf( child ) );
        break;
      }
    }
  }
}

void QgsLayerTreeGroup::removeChildren( int from, int count )
{
  removeChildrenRange( from, count );

  updateVisibilityFromChildren();
}

void QgsLayerTreeGroup::removeAllChildren()
{
  removeChildren( 0, mChildren.count() );
}

QgsLayerTreeLayer *QgsLayerTreeGroup::findLayer( const QString& layerId )
{
  foreach ( QgsLayerTreeNode* child, mChildren )
  {
    if ( QgsLayerTree::isLayer( child ) )
    {
      QgsLayerTreeLayer* childLayer = QgsLayerTree::toLayer( child );
      if ( childLayer->layerId() == layerId )
        return childLayer;
    }
    else if ( QgsLayerTree::isGroup( child ) )
    {
      QgsLayerTreeLayer* res = QgsLayerTree::toGroup( child )->findLayer( layerId );
      if ( res )
        return res;
    }
  }
  return 0;
}

QList<QgsLayerTreeLayer*> QgsLayerTreeGroup::findLayers() const
{
  QList<QgsLayerTreeLayer*> list;
  foreach ( QgsLayerTreeNode* child, mChildren )
  {
    if ( QgsLayerTree::isLayer( child ) )
      list << QgsLayerTree::toLayer( child );
    else if ( QgsLayerTree::isGroup( child ) )
      list << QgsLayerTree::toGroup( child )->findLayers();
  }
  return list;
}

QgsLayerTreeGroup* QgsLayerTreeGroup::findGroup( const QString& name )
{
  foreach ( QgsLayerTreeNode* child, mChildren )
  {
    if ( QgsLayerTree::isGroup( child ) )
    {
      QgsLayerTreeGroup* childGroup = QgsLayerTree::toGroup( child );
      if ( childGroup->name() == name )
        return childGroup;
      else
      {
        QgsLayerTreeGroup* grp = childGroup->findGroup( name );
        if ( grp )
          return grp;
      }
    }
  }
  return 0;
}

QgsLayerTreeGroup* QgsLayerTreeGroup::readXML( QDomElement& element )
{
  if ( element.tagName() != "layer-tree-group" )
    return 0;

  QString name = element.attribute( "name" );
  bool isExpanded = ( element.attribute( "expanded", "1" ) == "1" );
  Qt::CheckState checked = QgsLayerTreeUtils::checkStateFromXml( element.attribute( "checked" ) );

  QgsLayerTreeGroup* groupNode = new QgsLayerTreeGroup( name, checked );
  groupNode->setExpanded( isExpanded );

  groupNode->readCommonXML( element );

  groupNode->readChildrenFromXML( element );

  return groupNode;
}

void QgsLayerTreeGroup::writeXML( QDomElement& parentElement )
{
  QDomDocument doc = parentElement.ownerDocument();
  QDomElement elem = doc.createElement( "layer-tree-group" );
  elem.setAttribute( "name", mName );
  elem.setAttribute( "expanded", mExpanded ? "1" : "0" );
  elem.setAttribute( "checked", QgsLayerTreeUtils::checkStateToXml( mChecked ) );

  writeCommonXML( elem );

  foreach ( QgsLayerTreeNode* node, mChildren )
    node->writeXML( elem );

  parentElement.appendChild( elem );
}

void QgsLayerTreeGroup::readChildrenFromXML( QDomElement& element )
{
  QDomElement childElem = element.firstChildElement();
  while ( !childElem.isNull() )
  {
    QgsLayerTreeNode* newNode = QgsLayerTreeNode::readXML( childElem );
    if ( newNode )
      addChildNode( newNode );

    childElem = childElem.nextSiblingElement();
  }
}

QString QgsLayerTreeGroup::dump() const
{
  QString header = QString( "GROUP: %1 visible=%2 expanded=%3\n" ).arg( name() ).arg( mChecked ).arg( mExpanded );
  QStringList childrenDump;
  foreach ( QgsLayerTreeNode* node, mChildren )
    childrenDump << node->dump().split( "\n" );
  for ( int i = 0; i < childrenDump.count(); ++i )
    childrenDump[i].prepend( "  " );
  return header + childrenDump.join( "\n" );
}

QgsLayerTreeNode* QgsLayerTreeGroup::clone() const
{
  return new QgsLayerTreeGroup( *this );
}

void QgsLayerTreeGroup::setVisible( Qt::CheckState state )
{
  if ( mChecked == state )
    return;

  mChecked = state;
  emit visibilityChanged( this, state );

  if ( mChecked == Qt::Unchecked || mChecked == Qt::Checked )
  {
    mChangingChildVisibility = true; // guard against running again setVisible() triggered from children

    // update children to have the correct visibility
    foreach ( QgsLayerTreeNode* child, mChildren )
    {
      if ( QgsLayerTree::isGroup( child ) )
        QgsLayerTree::toGroup( child )->setVisible( mChecked );
      else if ( QgsLayerTree::isLayer( child ) )
        QgsLayerTree::toLayer( child )->setVisible( mChecked );
    }

    mChangingChildVisibility = false;
  }
}

QStringList QgsLayerTreeGroup::childLayerIds() const
{
  QStringList lst;
  foreach ( QgsLayerTreeNode* child, mChildren )
  {
    if ( QgsLayerTree::isGroup( child ) )
      lst << QgsLayerTree::toGroup( child )->childLayerIds();
    else if ( QgsLayerTree::isLayer( child ) )
      lst << QgsLayerTree::toLayer( child )->layerId();
  }
  return lst;
}


void QgsLayerTreeGroup::layerDestroyed()
{
  //QgsMapLayer* layer = static_cast<QgsMapLayer*>( sender() );
  //removeLayer( layer );
}

void QgsLayerTreeGroup::nodeVisibilityChanged( QgsLayerTreeNode* node )
{
  if ( mChildren.indexOf( node ) != -1 )
    updateVisibilityFromChildren();
}

void QgsLayerTreeGroup::updateVisibilityFromChildren()
{
  if ( mChangingChildVisibility )
    return;

  if ( mChildren.count() == 0 )
    return;

  bool hasVisible = false, hasHidden = false;

  foreach ( QgsLayerTreeNode* child, mChildren )
  {
    if ( QgsLayerTree::isLayer( child ) )
    {
      bool layerVisible = QgsLayerTree::toLayer( child )->isVisible() == Qt::Checked;
      if ( layerVisible ) hasVisible = true;
      if ( !layerVisible ) hasHidden = true;
    }
    else if ( QgsLayerTree::isGroup( child ) )
    {
      Qt::CheckState state = QgsLayerTree::toGroup( child )->isVisible();
      if ( state == Qt::Checked || state == Qt::PartiallyChecked ) hasVisible = true;
      if ( state == Qt::Unchecked || state == Qt::PartiallyChecked ) hasHidden = true;
    }
  }

  Qt::CheckState newState;
  if ( hasVisible && !hasHidden )
    newState = Qt::Checked;
  else if ( hasHidden && !hasVisible )
    newState = Qt::Unchecked;
  else
    newState = Qt::PartiallyChecked;

  setVisible( newState );
}

