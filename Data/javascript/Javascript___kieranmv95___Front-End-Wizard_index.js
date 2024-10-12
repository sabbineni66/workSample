import { combineReducers } from 'redux';
import linksReducer from './linksReducer';


export default combineReducers({
  links: linksReducer
})