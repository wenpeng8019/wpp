/**
 * SQTP (Structured Query Transfer Protocol) Client Library
 * Fetch API Implementation
 * 
 * Modern Promise-native implementation of SQTP/1.0 protocol
 * Uses the Fetch API for HTTP requests
 * 
 * @version 1.0.0
 * @license MIT
 */

(function(root, factory) {
  if (typeof define === 'function' && define.amd) {
    define([], factory);
  } else if (typeof module === 'object' && module.exports) {
    module.exports = factory();
  } else {
    root.SQTP = factory();
  }
}(typeof self !== 'undefined' ? self : this, function() {
  'use strict';

  /**
   * SQTP Client Constructor
   * @param {string} baseUrl - Base URL of the SQTP server (e.g., 'http://localhost:8080/db/main')
   * @param {object} options - Configuration options
   * @param {string} options.protocol - Protocol version (default: 'SQTP/1.0')
   * @param {number} options.timeout - Request timeout in milliseconds (default: 30000)
   * @param {boolean} options.debug - Enable debug logging (default: false)
   */
  function SQTP(baseUrl, options) {
    this.baseUrl = baseUrl.replace(/\/$/, ''); // Remove trailing slash
    this.options = Object.assign({
      protocol: 'SQTP/1.0',
      timeout: 30000,
      debug: false
    }, options || {});
  }

  /**
   * Internal method to execute HTTP request using Fetch API
   * @private
   */
  SQTP.prototype._request = function(method, fragment, headers, body) {
    const self = this;
    const url = this.baseUrl + (fragment || '');
    
    // Add SQTP- prefix to method
    const sqtpMethod = 'SQTP-' + method;
    
    // Build headers object - use plain object instead of Headers to avoid validation issues
    const fetchHeaders = {};
    if (headers) {
      Object.keys(headers).forEach(function(key) {
        const value = headers[key];
        if (value !== undefined && value !== null) {
          // Handle array values (multiple headers with same name)
          // Note: Fetch API with plain object doesn't support multiple headers with same name
          // For arrays, we'll use comma-separated values
          if (Array.isArray(value)) {
            fetchHeaders[key] = value.map(function(v) { return String(v); }).join(', ');
          } else {
            fetchHeaders[key] = String(value);
          }
        }
      });
    }
    if (body && !fetchHeaders['Content-Type']) {
      fetchHeaders['Content-Type'] = 'application/json';
    }

    // Debug logging with full protocol details
    if (this.options.debug) {
      console.group('%c[SQTP Protocol] ' + method + ' Request', 'color: #007bff; font-weight: bold');
      console.log('%cMethod:', 'color: #28a745; font-weight: bold', method);
      console.log('%cURL:', 'color: #28a745; font-weight: bold', url);
      if (headers && Object.keys(headers).length > 0) {
        console.log('%cHeaders:', 'color: #28a745; font-weight: bold');
        Object.keys(headers).forEach(function(key) {
          if (headers[key] !== undefined && headers[key] !== null) {
            console.log('  ' + key + ':', headers[key]);
          }
        });
      }
      if (body) {
        console.log('%cBody:', 'color: #28a745; font-weight: bold', 
          typeof body === 'string' ? body : JSON.stringify(body, null, 2));
      }
      console.groupEnd();
    }

    // Create fetch options
    const fetchOptions = {
      method: sqtpMethod,
      headers: fetchHeaders
    };

    if (body) {
      fetchOptions.body = typeof body === 'string' ? body : JSON.stringify(body);
    }

    // Create abort controller for timeout
    const controller = new AbortController();
    const timeoutId = setTimeout(function() {
      controller.abort();
    }, this.options.timeout);
    fetchOptions.signal = controller.signal;

    // Execute fetch request
    return fetch(url, fetchOptions)
      .then(function(response) {
        clearTimeout(timeoutId);
        
        // Extract response headers
        // Note: Headers object lowercases all keys, so we need to restore proper casing
        const responseHeaders = {};
        const headerCaseMap = {
          'x-sqtp-changes': 'X-SQTP-Changes',
          'x-sqtp-last-insert-id': 'X-SQTP-Last-Insert-Id'
        };
        response.headers.forEach(function(value, key) {
          const properKey = headerCaseMap[key] || key;
          responseHeaders[properKey] = value;
        });

        // Check for HTTP errors
        if (!response.ok) {
          return response.text().then(function(errorText) {
            if (self.options.debug) {
              console.group('%c[SQTP Protocol] ' + method + ' Response', 'color: #17a2b8; font-weight: bold');
              console.log('%cStatus:', 'color: #dc3545; font-weight: bold', response.status);
              if (Object.keys(responseHeaders).length > 0) {
                console.log('%cResponse Headers:', 'color: #28a745; font-weight: bold');
                Object.keys(responseHeaders).forEach(function(key) {
                  console.log('  ' + key + ':', responseHeaders[key]);
                });
              }
              console.log('%cError:', 'color: #dc3545; font-weight: bold', errorText);
              console.groupEnd();
            }
            throw new Error('HTTP ' + response.status + ': ' + (errorText || response.statusText));
          });
        }

        // Parse JSON response
        return response.json()
          .then(function(data) {
            if (self.options.debug) {
              console.group('%c[SQTP Protocol] ' + method + ' Response', 'color: #17a2b8; font-weight: bold');
              console.log('%cStatus:', 'color: #28a745; font-weight: bold', response.status);
              if (Object.keys(responseHeaders).length > 0) {
                console.log('%cResponse Headers:', 'color: #28a745; font-weight: bold');
                Object.keys(responseHeaders).forEach(function(key) {
                  console.log('  ' + key + ':', responseHeaders[key]);
                });
              }
              if (data) {
                console.log('%cResponse Data:', 'color: #28a745; font-weight: bold');
                if (typeof data === 'object') {
                  if (Array.isArray(data)) {
                    console.log('  Array with ' + data.length + ' items');
                    if (data.length > 0) {
                      console.log('  First item:', data[0]);
                    }
                  } else {
                    console.log(data);
                  }
                } else {
                  console.log(data);
                }
              }
              console.groupEnd();
            }
            return {
              data: data,
              headers: responseHeaders,
              status: response.status
            };
          })
          .catch(function(err) {
            // If JSON parsing fails, return empty data
            if (err instanceof SyntaxError) {
              return {
                data: null,
                headers: responseHeaders,
                status: response.status
              };
            }
            throw err;
          });
      })
      .catch(function(error) {
        clearTimeout(timeoutId);
        
        if (error.name === 'AbortError') {
          throw new Error('Request timeout after ' + self.options.timeout + 'ms');
        }
        
        // Only log network errors here (HTTP errors already logged in protocol response)
        if (self.options.debug && !error.message.startsWith('HTTP ')) {
          console.error('[SQTP Debug] Network Error:', error);
        }
        
        throw error;
      });
  };

  // ==================== SELECT Builder ====================
  
  function SelectBuilder(sqtp, tableName) {
    this.sqtp = sqtp;
    this.tableName = tableName;
    this.headers = {};
    this.whereInData = {};
  }

  SelectBuilder.prototype.columns = function() {
    this.headers['COLUMN'] = Array.prototype.slice.call(arguments).join(', ');
    return this;
  };

  SelectBuilder.prototype.where = function(condition) {
    if (!this.headers['WHERE']) {
      this.headers['WHERE'] = [];
    }
    this.headers['WHERE'].push(condition);
    return this;
  };

  SelectBuilder.prototype.whereIn = function(column, values) {
    if (!this.whereInData) {
      this.whereInData = {};
    }
    this.whereInData[column] = values;
    return this;
  };

  SelectBuilder.prototype.orderBy = function() {
    this.headers['ORDER-BY'] = Array.prototype.slice.call(arguments).join(' ');
    return this;
  };

  SelectBuilder.prototype.groupBy = function() {
    this.headers['GROUP-BY'] = Array.prototype.slice.call(arguments).join(', ');
    return this;
  };

  SelectBuilder.prototype.having = function(condition) {
    this.headers['HAVING'] = condition;
    return this;
  };

  SelectBuilder.prototype.limit = function(count) {
    this.headers['LIMIT'] = String(count);
    return this;
  };

  SelectBuilder.prototype.offset = function(count) {
    this.headers['OFFSET'] = String(count);
    return this;
  };

  SelectBuilder.prototype.distinct = function() {
    this.headers['DISTINCT'] = 'true';
    return this;
  };

  SelectBuilder.prototype.view = function(format) {
    this.headers['VIEW'] = format;
    return this;
  };

  SelectBuilder.prototype.join = function(table, condition) {
    if (!this.headers['JOIN']) {
      this.headers['JOIN'] = [];
    }
    this.headers['JOIN'].push(table + ' ON ' + condition);
    return this;
  };

  SelectBuilder.prototype.leftJoin = function(table, condition) {
    if (!this.headers['LEFT-JOIN']) {
      this.headers['LEFT-JOIN'] = [];
    }
    this.headers['LEFT-JOIN'].push(table + ' ON ' + condition);
    return this;
  };

  SelectBuilder.prototype.execute = function() {
    var body = null;
    
    // Add FROM header (required by server)
    this.headers['FROM'] = this.tableName;
    
    // Convert WHERE array to single string if only one condition
    if (this.headers['WHERE'] && Array.isArray(this.headers['WHERE'])) {
      if (this.headers['WHERE'].length === 1) {
        this.headers['WHERE'] = this.headers['WHERE'][0];
      }
      // For multiple WHERE conditions, keep as array
    }
    
    // Convert JOIN arrays to single strings if needed
    if (this.headers['JOIN'] && Array.isArray(this.headers['JOIN'])) {
      if (this.headers['JOIN'].length === 1) {
        this.headers['JOIN'] = this.headers['JOIN'][0];
      }
    }
    
    if (this.headers['LEFT-JOIN'] && Array.isArray(this.headers['LEFT-JOIN'])) {
      if (this.headers['LEFT-JOIN'].length === 1) {
        this.headers['LEFT-JOIN'] = this.headers['LEFT-JOIN'][0];
      }
    }
    
    if (this.whereInData) {
      var whereInKeys = Object.keys(this.whereInData);
      if (whereInKeys.length > 0) {
        if (whereInKeys.length === 1) {
          // Single WHERE-IN: header is column name, body is array
          this.headers['WHERE-IN'] = whereInKeys[0];
          body = this.whereInData[whereInKeys[0]];
        } else {
          // Multiple WHERE-IN: header is array of columns, body is object
          this.headers['WHERE-IN'] = whereInKeys;
          body = this.whereInData;
        }
      }
    }
    
    return this.sqtp._request('SELECT', '', this.headers, body);
  };

  // ==================== INSERT Builder ====================
  
  function InsertBuilder(sqtp, tableName) {
    this.sqtp = sqtp;
    this.tableName = tableName;
    this.headers = {};
    this.data = null;
  }

  InsertBuilder.prototype.values = function(data) {
    this.data = data;
    return this;
  };

  InsertBuilder.prototype.ifNotExists = function(enabled) {
    if (enabled) {
      this.headers['IF-NOT-EXISTS'] = 'true';
    }
    return this;
  };

  InsertBuilder.prototype.execute = function() {
    if (!this.data) {
      return Promise.reject(new Error('INSERT requires values'));
    }
    // Add TABLE header (required by server)
    this.headers['TABLE'] = this.tableName;
    return this.sqtp._request('INSERT', '', this.headers, this.data);
  };

  // ==================== UPDATE Builder ====================
  
  function UpdateBuilder(sqtp, tableName) {
    this.sqtp = sqtp;
    this.tableName = tableName;
    this.headers = {};
    this.data = null;
    this.whereInData = {};
  }

  UpdateBuilder.prototype.set = function(data) {
    this.data = data;
    return this;
  };

  UpdateBuilder.prototype.where = function(condition) {
    if (!this.headers['WHERE']) {
      this.headers['WHERE'] = [];
    }
    this.headers['WHERE'].push(condition);
    return this;
  };

  UpdateBuilder.prototype.whereIn = function(column, values) {
    if (!this.whereInData) {
      this.whereInData = {};
    }
    this.whereInData[column] = values;
    return this;
  };

  UpdateBuilder.prototype.execute = function() {
    if (!this.data) {
      return Promise.reject(new Error('UPDATE requires set() data'));
    }
    
    // WHERE or WHERE-IN is mandatory (use '*' for full table update)
    var hasWhere = this.headers['WHERE'] && this.headers['WHERE'].length > 0;
    var hasWhereIn = this.whereInData && Object.keys(this.whereInData).length > 0;
    if (!hasWhere && !hasWhereIn) {
      return Promise.reject(new Error('WHERE clause is required for UPDATE. Use where("*") for full table update.'));
    }
    
    // WHERE-IN
    if (this.whereInData) {
      var whereInKeys = Object.keys(this.whereInData);
      if (whereInKeys.length > 0) {
        whereInKeys.forEach(function(key) {
          if (!this.headers['WHERE-IN']) {
            this.headers['WHERE-IN'] = key;
          } else {
            var existing = this.headers['WHERE-IN'];
            delete this.headers['WHERE-IN'];
            this.headers['WHERE-IN'] = [existing, key];
          }
        }, this);
      }
    }
    
    // Body contains both values and WHERE-IN data
    var body = Object.assign({}, this.data);
    if (this.whereInData) {
      var whereInKeys = Object.keys(this.whereInData);
      whereInKeys.forEach(function(key) {
        body[key] = this.whereInData[key];
      }, this);
    }
    
    // Add TABLE header (required by server)
    this.headers['TABLE'] = this.tableName;
    return this.sqtp._request('UPDATE', '', this.headers, body);
  };

  // ==================== UPSERT Builder ====================
  
  function UpsertBuilder(sqtp, tableName) {
    this.sqtp = sqtp;
    this.tableName = tableName;
    this.headers = {};
    this.data = null;
  }

  UpsertBuilder.prototype.values = function(data) {
    this.data = data;
    return this;
  };

  UpsertBuilder.prototype.key = function() {
    this.headers['KEY'] = Array.prototype.slice.call(arguments).join(' ');
    return this;
  };

  UpsertBuilder.prototype.execute = function() {
    if (!this.data) {
      return Promise.reject(new Error('UPSERT requires values'));
    }
    // Add TABLE header (required by server)
    this.headers['TABLE'] = this.tableName;
    return this.sqtp._request('UPSERT', '', this.headers, this.data);
  };

  // ==================== DELETE Builder ====================
  
  function DeleteBuilder(sqtp, tableName) {
    this.sqtp = sqtp;
    this.tableName = tableName;
    this.headers = {};
    this.whereInData = {};
  }

  DeleteBuilder.prototype.where = function(condition) {
    if (!this.headers['WHERE']) {
      this.headers['WHERE'] = [];
    }
    this.headers['WHERE'].push(condition);
    return this;
  };

  DeleteBuilder.prototype.whereIn = function(column, values) {
    if (!this.whereInData) {
      this.whereInData = {};
    }
    this.whereInData[column] = values;
    return this;
  };

  DeleteBuilder.prototype.execute = function() {
    // WHERE or WHERE-IN is mandatory (use '*' for full table delete)
    var hasWhere = this.headers['WHERE'] && this.headers['WHERE'].length > 0;
    var hasWhereIn = this.whereInData && Object.keys(this.whereInData).length > 0;
    if (!hasWhere && !hasWhereIn) {
      return Promise.reject(new Error('WHERE clause is required for DELETE. Use where("*") for full table delete.'));
    }
    
    // WHERE-IN
    var body = null;
    if (this.whereInData) {
      var whereInKeys = Object.keys(this.whereInData);
      if (whereInKeys.length > 0) {
        whereInKeys.forEach(function(key) {
          if (!this.headers['WHERE-IN']) {
            this.headers['WHERE-IN'] = key;
          } else {
            var existing = this.headers['WHERE-IN'];
            delete this.headers['WHERE-IN'];
            this.headers['WHERE-IN'] = [existing, key];
          }
        }, this);
        
        if (whereInKeys.length === 1) {
          body = this.whereInData[whereInKeys[0]];
        } else {
          body = this.whereInData;
        }
      }
    }
    
    // Add TABLE header (required by server)
    this.headers['TABLE'] = this.tableName;
    return this.sqtp._request('DELETE', '', this.headers, body);
  };

  // ==================== CREATE TABLE Builder ====================
  
  function CreateTableBuilder(sqtp, tableName) {
    this.sqtp = sqtp;
    this.tableName = tableName;
    this.headers = { 'NAME': tableName };
  }

  CreateTableBuilder.prototype.column = function(name, type) {
    if (!this.headers['COLUMN']) {
      this.headers['COLUMN'] = [];
    }
    var columnDef = [name, type];
    for (var i = 2; i < arguments.length; i++) {
      columnDef.push(arguments[i]);
    }
    this.headers['COLUMN'].push(columnDef.join(' '));
    return this;
  };

  CreateTableBuilder.prototype.primaryKey = function() {
    this.headers['PRIMARY-KEY'] = Array.prototype.slice.call(arguments).join(' ');
    return this;
  };

  CreateTableBuilder.prototype.notNull = function() {
    this.headers['NOT-NULL'] = Array.prototype.slice.call(arguments).join(' ');
    return this;
  };

  CreateTableBuilder.prototype.unique = function() {
    if (!this.headers['UNIQUE']) {
      this.headers['UNIQUE'] = [];
    }
    this.headers['UNIQUE'].push(Array.prototype.slice.call(arguments).join(' '));
    return this;
  };

  CreateTableBuilder.prototype.autoinc = function(columnName) {
    this.headers['AUTOINC'] = columnName;
    return this;
  };

  CreateTableBuilder.prototype.foreignKey = function(definition) {
    if (!this.headers['FOREIGN-KEY']) {
      this.headers['FOREIGN-KEY'] = [];
    }
    this.headers['FOREIGN-KEY'].push(definition);
    return this;
  };

  CreateTableBuilder.prototype.ifNotExists = function(enabled) {
    if (enabled) {
      this.headers['IF-NOT-EXISTS'] = 'true';
    }
    return this;
  };

  CreateTableBuilder.prototype.type = function(tableType) {
    this.headers['TYPE'] = tableType;
    return this;
  };

  CreateTableBuilder.prototype.withoutRowid = function(enabled) {
    if (enabled) {
      this.headers['WITHOUT-ROWID'] = 'true';
    }
    return this;
  };

  CreateTableBuilder.prototype.execute = function() {
    return this.sqtp._request('CREATE', '#table', this.headers, null);
  };

  // ==================== Public API Methods ====================

  SQTP.prototype.select = function(tableName) {
    return new SelectBuilder(this, tableName);
  };

  SQTP.prototype.insert = function(tableName) {
    return new InsertBuilder(this, tableName);
  };

  SQTP.prototype.update = function(tableName) {
    return new UpdateBuilder(this, tableName);
  };

  SQTP.prototype.upsert = function(tableName) {
    return new UpsertBuilder(this, tableName);
  };

  SQTP.prototype.delete = function(tableName) {
    return new DeleteBuilder(this, tableName);
  };

  SQTP.prototype.createTable = function(tableName) {
    return new CreateTableBuilder(this, tableName);
  };

  SQTP.prototype.dropTable = function(tableName, ifExists) {
    var headers = { 'NAME': tableName };
    if (ifExists) {
      headers['IF-EXISTS'] = 'true';
    }
    return this._request('DROP', '#table', headers, null);
  };

  // ==================== Transaction Methods ====================

  SQTP.prototype.begin = function() {
    return this._request('BEGIN', '', {}, null);
  };

  SQTP.prototype.commit = function() {
    return this._request('COMMIT', '', {}, null);
  };

  SQTP.prototype.rollback = function() {
    return this._request('ROLLBACK', '', {}, null);
  };

  SQTP.prototype.savepoint = function(name) {
    return this._request('SAVEPOINT', '', { 'NAME': name }, null);
  };

  return SQTP;
}));
