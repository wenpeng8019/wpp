/**
 * SQTP/1.0 Client Library - XMLHttpRequest Implementation
 * Structured Query Transfer Protocol JavaScript SDK
 * 
 * @version 1.0.0
 * @license MIT
 */

(function(global) {
  'use strict';

  /**
   * SQTP Client Constructor
   * @param {string} baseUrl - Database endpoint URL (e.g., 'http://localhost:8080/db/main')
   * @param {object} options - Configuration options
   */
  function SQTP(baseUrl, options) {
    if (!(this instanceof SQTP)) {
      return new SQTP(baseUrl, options);
    }

    this.baseUrl = baseUrl.replace(/\/$/, ''); // Remove trailing slash
    this.options = Object.assign({
      protocol: 'SQTP/1.0',
      timeout: 30000,
      debug: false
    }, options || {});
  }

  /**
   * Internal XHR request wrapper
   * @private
   */
  SQTP.prototype._request = function(method, fragment, headers, body) {
    const self = this;
    
    return new Promise(function(resolve, reject) {
      const xhr = new XMLHttpRequest();
      const url = fragment ? self.baseUrl + fragment : self.baseUrl;
      
      // Add SQTP- prefix to method
      const sqtpMethod = 'SQTP-' + method;
      
      xhr.open(sqtpMethod, url, true);
      xhr.timeout = self.options.timeout;
      
      // Set headers
      if (headers) {
        Object.keys(headers).forEach(function(key) {
          if (headers[key] !== undefined && headers[key] !== null) {
            xhr.setRequestHeader(key, headers[key]);
          }
        });
      }
      
      // Debug logging with full protocol details
      if (self.options.debug) {
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
      
      xhr.onload = function() {
        const result = {
          status: xhr.status,
          headers: {},
          data: null
        };
        
        // Parse response headers
        const headerStr = xhr.getAllResponseHeaders();
        headerStr.trim().split('\r\n').forEach(function(line) {
          const parts = line.split(': ');
          if (parts.length === 2) {
            result.headers[parts[0]] = parts[1];
          }
        });
        
        // Parse response body
        if (xhr.responseText) {
          try {
            result.data = JSON.parse(xhr.responseText);
          } catch (e) {
            result.data = xhr.responseText;
          }
        }
        
        if (self.options.debug) {
          console.group('%c[SQTP Protocol] ' + method + ' Response', 'color: #17a2b8; font-weight: bold');
          console.log('%cStatus:', 'color: ' + (xhr.status >= 200 && xhr.status < 300 ? '#28a745' : '#dc3545') + '; font-weight: bold', xhr.status);
          if (Object.keys(result.headers).length > 0) {
            console.log('%cResponse Headers:', 'color: #28a745; font-weight: bold');
            Object.keys(result.headers).forEach(function(key) {
              console.log('  ' + key + ':', result.headers[key]);
            });
          }
          if (result.data) {
            console.log('%cResponse Data:', 'color: #28a745; font-weight: bold');
            if (typeof result.data === 'object') {
              if (Array.isArray(result.data)) {
                console.log('  Array with ' + result.data.length + ' items');
                if (result.data.length > 0) {
                  console.log('  First item:', result.data[0]);
                }
              } else {
                console.log(result.data);
              }
            } else {
              console.log(result.data);
            }
          }
          console.groupEnd();
        }
        
        if (xhr.status >= 200 && xhr.status < 300) {
          resolve(result);
        } else {
          const error = new Error(result.data || 'Request failed');
          error.status = xhr.status;
          error.response = result;
          reject(error);
        }
      };
      
      xhr.onerror = function() {
        reject(new Error('Network error'));
      };
      
      xhr.ontimeout = function() {
        reject(new Error('Request timeout'));
      };
      
      // Send request
      if (body !== undefined && body !== null) {
        xhr.send(typeof body === 'string' ? body : JSON.stringify(body));
      } else {
        xhr.send();
      }
    });
  };

  // ==================== SELECT Query Builder ====================

  function SelectBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._columns = null;
    this._where = [];
    this._whereIn = {};
    this._orderBy = null;
    this._groupBy = null;
    this._having = null;
    this._limit = null;
    this._offset = null;
    this._distinct = false;
    this._view = 'object';
    this._join = null;
  }

  SelectBuilder.prototype.columns = function(...cols) {
    this._columns = cols.join(', ');
    return this;
  };

  SelectBuilder.prototype.where = function(condition) {
    this._where.push(condition);
    return this;
  };

  SelectBuilder.prototype.whereIn = function(column, values) {
    if (!this._whereIn[column]) {
      this._whereIn[column] = [];
    }
    this._whereIn[column] = values;
    return this;
  };

  SelectBuilder.prototype.orderBy = function(order) {
    this._orderBy = order;
    return this;
  };

  SelectBuilder.prototype.groupBy = function(...cols) {
    this._groupBy = cols.join(', ');
    return this;
  };

  SelectBuilder.prototype.having = function(condition) {
    this._having = condition;
    return this;
  };

  SelectBuilder.prototype.limit = function(n) {
    this._limit = n;
    return this;
  };

  SelectBuilder.prototype.offset = function(n) {
    this._offset = n;
    return this;
  };

  SelectBuilder.prototype.distinct = function(flag) {
    this._distinct = flag !== false;
    return this;
  };

  SelectBuilder.prototype.view = function(format) {
    this._view = format; // 'object', 'row', 'column'
    return this;
  };

  SelectBuilder.prototype.join = function(joinClause) {
    this._join = joinClause;
    return this;
  };

  SelectBuilder.prototype.leftJoin = function(table, on) {
    const joinStr = 'LEFT JOIN ' + table + ' ON ' + on;
    this._join = this._join ? this._join + ' ' + joinStr : joinStr;
    return this;
  };

  SelectBuilder.prototype.execute = function() {
    const headers = {};
    let body = null;

    // Columns
    if (this._columns) {
      headers['COLUMNS'] = this._columns;
    }

    // FROM or FROM-JOIN
    if (this._join) {
      headers['FROM-JOIN'] = this._table + ' ' + this._join;
    } else {
      headers['FROM'] = this._table;
    }

    // WHERE conditions
    this._where.forEach(function(condition) {
      if (!headers['WHERE']) {
        headers['WHERE'] = condition;
      } else {
        // Multiple WHERE headers
        const existing = headers['WHERE'];
        delete headers['WHERE'];
        headers['WHERE'] = [existing, condition];
      }
    });

    // WHERE-IN
    const whereInKeys = Object.keys(this._whereIn);
    if (whereInKeys.length > 0) {
      if (whereInKeys.length === 1) {
        headers['WHERE-IN'] = whereInKeys[0];
        body = this._whereIn[whereInKeys[0]];
      } else {
        whereInKeys.forEach(function(key) {
          if (!headers['WHERE-IN']) {
            headers['WHERE-IN'] = key;
          } else {
            const existing = headers['WHERE-IN'];
            delete headers['WHERE-IN'];
            headers['WHERE-IN'] = [existing, key];
          }
        });
        body = this._whereIn;
      }
      headers['Content-Type'] = 'application/json; charset=utf-8';
    }

    // Other clauses
    if (this._groupBy) headers['GROUP-BY'] = this._groupBy;
    if (this._having) headers['HAVING'] = this._having;
    if (this._orderBy) headers['ORDER-BY'] = this._orderBy;
    if (this._limit !== null) headers['LIMIT'] = String(this._limit);
    if (this._offset !== null) headers['OFFSET'] = String(this._offset);
    if (this._distinct) headers['DISTINCT'] = 'true';
    if (this._view !== 'object') headers['X-SQTP-VIEW'] = this._view;

    // Convert single-element arrays to strings for WHERE and WHERE-IN
    if (Array.isArray(headers['WHERE']) && headers['WHERE'].length === 1) {
      headers['WHERE'] = headers['WHERE'][0];
    }
    if (Array.isArray(headers['WHERE-IN']) && headers['WHERE-IN'].length === 1) {
      headers['WHERE-IN'] = headers['WHERE-IN'][0];
    }

    return this.client._request('SELECT', '', headers, body);
  };

  SQTP.prototype.select = function(table) {
    return new SelectBuilder(this, table);
  };

  // ==================== INSERT Builder ====================

  function InsertBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._values = null;
    this._ifNotExists = false;
  }

  InsertBuilder.prototype.values = function(data) {
    this._values = data;
    return this;
  };

  InsertBuilder.prototype.ifNotExists = function(flag) {
    this._ifNotExists = flag !== false;
    return this;
  };

  InsertBuilder.prototype.execute = function() {
    const headers = {
      'TABLE': this._table,
      'Content-Type': 'application/json; charset=utf-8'
    };

    if (this._ifNotExists) {
      headers['IF-NOT-EXISTS'] = 'true';
    }

    return this.client._request('INSERT', '', headers, this._values);
  };

  SQTP.prototype.insert = function(table) {
    return new InsertBuilder(this, table);
  };

  // ==================== UPDATE Builder ====================

  function UpdateBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._values = null;
    this._where = [];
    this._whereIn = {};
  }

  UpdateBuilder.prototype.set = function(data) {
    this._values = data;
    return this;
  };

  UpdateBuilder.prototype.where = function(condition) {
    this._where.push(condition);
    return this;
  };

  UpdateBuilder.prototype.whereIn = function(column, values) {
    this._whereIn[column] = values;
    return this;
  };

  UpdateBuilder.prototype.execute = function() {
    const headers = {
      'TABLE': this._table,
      'Content-Type': 'application/json; charset=utf-8'
    };

    // WHERE is mandatory (use '*' for full table update)
    if (this._where.length === 0 && Object.keys(this._whereIn).length === 0) {
      return Promise.reject(new Error('WHERE clause is required for UPDATE. Use where("*") for full table update.'));
    }

    // WHERE conditions
    this._where.forEach(function(condition) {
      if (!headers['WHERE']) {
        headers['WHERE'] = condition;
      } else {
        const existing = headers['WHERE'];
        delete headers['WHERE'];
        headers['WHERE'] = [existing, condition];
      }
    });

    // WHERE-IN
    const whereInKeys = Object.keys(this._whereIn);
    if (whereInKeys.length > 0) {
      whereInKeys.forEach(function(key) {
        if (!headers['WHERE-IN']) {
          headers['WHERE-IN'] = key;
        } else {
          const existing = headers['WHERE-IN'];
          delete headers['WHERE-IN'];
          headers['WHERE-IN'] = [existing, key];
        }
      });
    }

    // Body contains both values and WHERE-IN data
    const body = Object.assign({}, this._values);
    if (whereInKeys.length > 0) {
      Object.keys(this._whereIn).forEach(function(key) {
        body[key] = this._whereIn[key];
      }, this);
    }

    return this.client._request('UPDATE', '', headers, body);
  };

  SQTP.prototype.update = function(table) {
    return new UpdateBuilder(this, table);
  };

  // ==================== UPSERT Builder ====================

  function UpsertBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._values = null;
    this._key = null;
  }

  UpsertBuilder.prototype.values = function(data) {
    this._values = data;
    return this;
  };

  UpsertBuilder.prototype.key = function(keyField) {
    this._key = keyField;
    return this;
  };

  UpsertBuilder.prototype.execute = function() {
    const headers = {
      'TABLE': this._table,
      'Content-Type': 'application/json; charset=utf-8'
    };

    if (this._key) {
      headers['KEY'] = this._key;
    }

    return this.client._request('UPSERT', '', headers, this._values);
  };

  SQTP.prototype.upsert = function(table) {
    return new UpsertBuilder(this, table);
  };

  // ==================== DELETE Builder ====================

  function DeleteBuilder(client, table) {
    this.client = client;
    this._table = table;
    this._where = [];
    this._whereIn = {};
  }

  DeleteBuilder.prototype.where = function(condition) {
    this._where.push(condition);
    return this;
  };

  DeleteBuilder.prototype.whereIn = function(column, values) {
    this._whereIn[column] = values;
    return this;
  };

  DeleteBuilder.prototype.execute = function() {
    const headers = {
      'TABLE': this._table
    };

    // WHERE is mandatory (use '*' for full table delete)
    if (this._where.length === 0 && Object.keys(this._whereIn).length === 0) {
      return Promise.reject(new Error('WHERE clause is required for DELETE. Use where("*") for full table delete.'));
    }

    // WHERE conditions
    this._where.forEach(function(condition) {
      if (!headers['WHERE']) {
        headers['WHERE'] = condition;
      } else {
        const existing = headers['WHERE'];
        delete headers['WHERE'];
        headers['WHERE'] = [existing, condition];
      }
    });

    // WHERE-IN
    const whereInKeys = Object.keys(this._whereIn);
    let body = null;
    if (whereInKeys.length > 0) {
      whereInKeys.forEach(function(key) {
        if (!headers['WHERE-IN']) {
          headers['WHERE-IN'] = key;
        } else {
          const existing = headers['WHERE-IN'];
          delete headers['WHERE-IN'];
          headers['WHERE-IN'] = [existing, key];
        }
      });
      headers['Content-Type'] = 'application/json; charset=utf-8';
      
      if (whereInKeys.length === 1) {
        body = this._whereIn[whereInKeys[0]];
      } else {
        body = this._whereIn;
      }
    }

    return this.client._request('DELETE', '', headers, body);
  };

  SQTP.prototype.delete = function(table) {
    return new DeleteBuilder(this, table);
  };

  // ==================== CREATE TABLE Builder ====================

  function CreateTableBuilder(client, tableName) {
    this.client = client;
    this._name = tableName;
    this._columns = [];
    this._primaryKey = null;
    this._notNull = [];
    this._unique = [];
    this._autoinc = null;
    this._foreignKeys = [];
    this._ifNotExists = false;
    this._type = null;
    this._withoutRowid = false;
  }

  CreateTableBuilder.prototype.column = function(name, type, ...constraints) {
    const col = name + ' ' + type;
    if (constraints.length > 0) {
      this._columns.push(col + ' ' + constraints.join(' '));
    } else {
      this._columns.push(col);
    }
    return this;
  };

  CreateTableBuilder.prototype.primaryKey = function(...cols) {
    this._primaryKey = cols.join(', ');
    return this;
  };

  CreateTableBuilder.prototype.notNull = function(...cols) {
    this._notNull = this._notNull.concat(cols);
    return this;
  };

  CreateTableBuilder.prototype.unique = function(...cols) {
    this._unique.push(cols.join(', '));
    return this;
  };

  CreateTableBuilder.prototype.autoinc = function(col) {
    this._autoinc = col;
    return this;
  };

  CreateTableBuilder.prototype.foreignKey = function(definition) {
    this._foreignKeys.push(definition);
    return this;
  };

  CreateTableBuilder.prototype.ifNotExists = function(flag) {
    this._ifNotExists = flag !== false;
    return this;
  };

  CreateTableBuilder.prototype.type = function(tableType) {
    this._type = tableType; // 'temporary' or 'memory'
    return this;
  };

  CreateTableBuilder.prototype.withoutRowid = function(flag) {
    this._withoutRowid = flag !== false;
    return this;
  };

  CreateTableBuilder.prototype.execute = function() {
    const headers = {
      'NAME': this._name
    };

    if (this._ifNotExists) headers['IF-NOT-EXISTS'] = 'true';
    if (this._type) headers['TYPE'] = this._type;

    // Add columns
    this._columns.forEach(function(col) {
      if (!headers['COLUMN']) {
        headers['COLUMN'] = col;
      } else {
        const existing = headers['COLUMN'];
        delete headers['COLUMN'];
        headers['COLUMN'] = [existing, col];
      }
    });

    if (this._primaryKey) headers['PRIMARY-KEY'] = this._primaryKey;
    if (this._notNull.length > 0) headers['NOT-NULL'] = this._notNull.join(', ');
    
    this._unique.forEach(function(u) {
      if (!headers['UNIQUE']) {
        headers['UNIQUE'] = u;
      } else {
        const existing = headers['UNIQUE'];
        delete headers['UNIQUE'];
        headers['UNIQUE'] = [existing, u];
      }
    });

    if (this._autoinc) headers['AUTOINC'] = this._autoinc;
    
    this._foreignKeys.forEach(function(fk) {
      if (!headers['FOREIGN-KEY']) {
        headers['FOREIGN-KEY'] = fk;
      } else {
        const existing = headers['FOREIGN-KEY'];
        delete headers['FOREIGN-KEY'];
        headers['FOREIGN-KEY'] = [existing, fk];
      }
    });

    if (this._withoutRowid) headers['WITHOUT-ROWID'] = 'true';

    return this.client._request('CREATE', '#table', headers, null);
  };

  SQTP.prototype.createTable = function(tableName) {
    return new CreateTableBuilder(this, tableName);
  };

  // ==================== DROP TABLE ====================

  SQTP.prototype.dropTable = function(tableName, ifExists) {
    const headers = {
      'NAME': tableName
    };
    if (ifExists) headers['IF-EXISTS'] = 'true';
    
    return this._request('DROP', '#table', headers, null);
  };

  // ==================== Transactions ====================

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

  // Export
  if (typeof module !== 'undefined' && module.exports) {
    module.exports = SQTP;
  } else {
    global.SQTP = SQTP;
  }

})(typeof window !== 'undefined' ? window : this);
