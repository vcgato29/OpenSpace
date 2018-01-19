export const splitURI = (URI) => {
  const indexForName = URI.indexOf('.');
  const name = URI.substring(0, indexForName != -1 ?
    indexForName : URI.length);
  URI = URI.substring(indexForName + 1, URI.length);

  return {name: name, URI: URI, lastOwner: URI.indexOf('.') === -1, lastNode: URI.length === 0}
}

export const getIdOfProperty = (URI) => {
  const indexForName = URI.lastIndexOf('.');
  return URI.substring(indexForName != -1 ?
    indexForName + 1 : 0, URI.length);

}

export const changePropertyOwnerState = (state, action) => {
    switch(action.type) {
      case 'SCENEGRAPH_START_LISTENING':
      case 'SCENEGRAPH_START_LISTENING':
        return {
          ...state,
          listeners: (action.type == 'SCENEGRAPH_START_LISTENING') ? 
          state.listeners + 1 : state.listeners - 1,
        }
      case 'SCENEGRAPH_UPDATE_PROPERTY':
        return {
          name: action.payload.node.name,
          properties: properties(undefined, action),
          subowners: action.payload.node.subowners.map(subowner => {
            const newAction = {
              ...action,
              payload: {
                node: subowner
              }
            }
            return propertyOwner(undefined, newAction);
          }),
          tag: (action.payload.node.tag === undefined) ? [] : action.payload.node.tag,
          listeners: 0,
        } 
      default:
       return state;
    }
    
}

const convertPointsBeforeSending = (position) => {
      let x = (position.x);
      let y = (600 - position.y);
      return {x: x, y: y};
  }

const keepCloning = (objectpassed) => {
    if (objectpassed === null || typeof objectpassed !== 'object') {
       return objectpassed;
    }
    // give temporary-storage the original obj's constructor
    var temporaryStorage = objectpassed.constructor();
    for (var key in objectpassed) {
      temporaryStorage[key] = keepCloning(objectpassed[key]);
    }
    return temporaryStorage;
  }

const convertEnvelopes = (envelopes) => {
    var convertedEnvelopes = keepCloning(envelopes);
    convertedEnvelopes = convertedEnvelopes.map(envelope =>
      Object.assign({},
        {points: envelope.points.map(point =>
            Object.assign({},
            { color : point.color,
              position : convertPointsBeforeSending(point.position),
            })
          )
        },
        {height:600},
        {width: 800},
      )
    )
    return JSON.stringify(convertedEnvelopes);
  }


const jsonToLua = (json) => {
    return json.replace('[', '').replace(']', '');
}

const traverseTree = (node, URI) => {
  const splittedURI = splitURI(URI);
  let tmpValue;
  if (splittedURI.lastNode) {
    node.properties.forEach(function(element) {
      if(element.id === splittedURI.URI) {
        tmpValue = element;
      }
    })
    return tmpValue;
  }
  else {
    node.subowners.forEach(function(element) {
      if( element.name === splittedURI.name) {
        tmpValue = traverseTree(element, splittedURI.URI);
        return tmpValue;
      }
    })
    return tmpValue;
  }
}

const findUpdatedProperty = (state, URI) => {
  let property;
  const splittedURI = splitURI(URI);
  let returnNode;
  state.forEach(function(element) {
    if( element.name === splittedURI.name) {
      returnNode = traverseTree(element, splittedURI.URI);
    }
  })
  return returnNode;
}