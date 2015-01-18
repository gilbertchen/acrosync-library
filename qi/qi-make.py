#!/usr/bin/env python

import getpass
import imp
import os
import optparse 
import platform
import re
import shlex
import subprocess
import sys
import time
import threading
import types
import Queue

PROJECT_FILE = "qi.prj"
SOURCE_FILES_SECTION = "[Source Files]"
INITIALIZATION_CODE_SECTION = "[Initialization Code]"
FINALIZATION_CODE_SECTION = "[Finalization Code]"
USER_DEFINED_FUNCTIONS_SECTION = "[User-Defined Functions]"

class Node:

    def __init__(self, name, targets, isFile, fileName, lineNumber):
        self.name       = name
        self.targets    = targets 
        self.children   = [] 
        self.isFile     = isFile 

        self.fileName   = fileName
        self.lineNumber = lineNumber 

    def addCode(self, line, newLine):
        if newLine:
            self.code.append(line)
        else:
            self.code[-1] += line

class QiError:

    def __init__(self, message, fileName = None, lineNumber = None):
        self.message = message 
        self.fileName = fileName
        self.lineNumber = lineNumber

class Builder:

    COMMAND_ASSIGNMENT = 1
    COMMAND_FUNCTIONCALL = 2
    COMMAND_EXTERNAL   = 4 

    ReservedActions = ["add", "delete", "list", "scan", "parse", "set", "unset", "init"]

    def __init__(self, rootDir, sourceHeaderMap,
                 initializationCode, finalizationCode, userDefinedModule, options):
        self.rootDir = rootDir
        self.sourceHeaderMap = sourceHeaderMap 
        self.initializationCode = initializationCode
        self.finalizationCode = finalizationCode 
        self.options = options
 
        self.headerSourceMap = {}
        self.baseSourceMap = {}  # Used to find the cpp file given a base name.
        for source, headers in sourceHeaderMap.iteritems():
            if headers:
                for header in headers:
                    if header in self.headerSourceMap.iterkeys():
                        self.headerSourceMap[header].append(source)
                    else:
                        self.headerSourceMap[header] = [source]
            else:
                base = os.path.splitext(source)[0]
                if base not in self.baseSourceMap.iterkeys():
                    self.baseSourceMap[base] = source
                else:
                    raise QiError("'%s' and '%s' share the same base name" % (source, self.baseSourceMap[base]))

        self.userDefinedFunctions = {}
        for name in dir(userDefinedModule):
            object = getattr(userDefinedModule, name, None)
            if isinstance(object, types.FunctionType):
                self.userDefinedFunctions[name] = object
        
        self.nodes = {}

        self.includePathRegex = re.compile(r"/(?:/|\*)qi:\s*includepath\s+<(.+)>")
        self.includeFileRegex = re.compile(r"(?://)?#\s*include\s*[\"<](.+)[\">]\s*(/(/|\*)qi:\s*ignore)?")
        self.oneLineQiCodeRegex = re.compile(r"//qi:\s*(.*)|/\*qi:\s*(.*)\*/")
        self.beginQiCodeRegex = re.compile(r"(/\*)?qi:\s*begin")
        self.endQiCodeRegex = re.compile(r"qi:\s*end\s*(\*/)?")

        self.lineInfoRegex = re.compile(r"([^,]+), (\d+): (.*)")
        self.ifRegex = re.compile(r"\s*if\s+(\$)?(\w+)\s*(~=|~|!~|==|=|!=)(.*)")
        self.ifdefRegex = re.compile(r"\s*if(n)?def\s+(\w+)")
        self.elseRegex = re.compile(r"\s*else")
        self.endifRegex = re.compile(r"\s*endif")
        self.assignmentRegex = re.compile(r"(\s*)(\w+)\s*(=|\+=|:=)(.*)")
        self.ruleRegex = re.compile(r"(\s*)(\w+)(\([^:]+\))?:(.*)")
        self.indentationRegex = re.compile(r"(\s*)")

        self.functionCallRegex = re.compile(r"\$\((\w+)\s+(.*)\)")
        self.actionSourcesRegex = re.compile(r"(?:\$)?(\w+)\((.*)\)$")
        self.stringParserRegex = re.compile(r"""(?P<action>(?<!\$)\$\w+\()|
                                                (?P<var1>(?<!\$)\$\w+)|
                                                (?P<var2>(?<!\$)\$\(\w+\))|
                                                (?P<funct>(?<!\$)\$\(\w+\s)|
                                                (?P<dollar>\$\$)|
                                                (?P<lparen>(?<!\\)\()|
                                                (?P<rparen>(?<!\\)\))""", re.VERBOSE)

        self.symbolTable = {}
        for variable in os.environ.iterkeys():
            self.symbolTable[variable] = os.environ[variable]

    def scan(self, source):
        sourceNode = self.findFileNode(source)
        if not sourceNode:
            sourceNode = self.addFileNode(source)
        if sourceNode.scanned:
            return sourceNode
 
        sourceNode.lineNumber = 0
        sourceNode.sanned = True

        if self.options.verbose:
            print self.getTime() + " Scanning %s" % source
      
        fullName = join(self.rootDir, source) 
        currentNode = sourceNode

        try:
            sourceNode.file = open(fullName)

            nodeStack = [sourceNode]
            includePaths = [self.rootDir]
            while (nodeStack):
               currentNode = nodeStack[-1]
               currentFile = currentNode.file
               keepReading = True
               while keepReading:
                   currentNode.lineNumber += 1
                   line = currentFile.readline()
                   if not line:
                       currentFile.close()
                       nodeStack.pop()    
                       break;

                   # Checking //includepath
                   m = self.includePathRegex.match(line)
                   if m:
                       path = os.path.normpath(join(self.rootDir, m.group(1).strip()))
                       if not os.path.exists(path):
                           print "Error: '%s' is not a valid path" % path
                       elif path not in includePaths:
                           includePaths.append(path)
                       continue
                    
                   # Checking '#include<file.h>'
                   m = self.includeFileRegex.match(line) 
                   if m and not m.group(2):
                       file = normalizePath(m.group(1))
                       header = None
                       for path in includePaths:
                           if os.path.exists(join(path, file)):
                               header = getStandardName(self.rootDir, join(path, file))
                               break
                       if not header:
                           continue
                       if header not in self.headerSourceMap.iterkeys():
                           base = os.path.splitext(file)[0]
                           if base in self.baseSourceMap.iterkeys():
                               source = self.baseSourceMap[base]
                               self.sourceHeaderMap[source].append(header)
                               self.headerSourceMap[header] = [source]
                       headerNode = self.findFileNode(header)
                       if not headerNode:
                           headerNode = self.addFileNode(header, mayNotExist = True)
                       self.setDependency(currentNode, headerNode)
                       currentNode.addCode(headerNode, newLine = True)
                       if not headerNode.scanned:
                           currentNode = headerNode
                           currentNode.file = open(join(self.rootDir, header))
                           currentNode.lineNumber = 0
                           currentNode.scanned = True
                           nodeStack.append(currentNode)
                           keepReading = False
                       continue

                   if self.beginQiCodeRegex.match(line):
                       continuation = False
                       while True:
                           currentNode.lineNumber += 1
                           line = currentFile.readline()
                           if not line or self.endQiCodeRegex.match(line):
                               break
                           line = line.rstrip("\n\r ")
                           if line.lstrip(" ").startswith("#"):
                               continuation = False
                               continue
                           backSlash = False
                           if line and line[-1] == "\\":
                               backSlash = True
                               line = line.rstrip("\\")
                           if continuation:
                               currentNode.addCode(line, False) 
                           else:
                               currentNode.addCode("%s, %i: %s" % (currentNode.name, currentNode.lineNumber, line), True)
                           continuation = backSlash

                   m = self.oneLineQiCodeRegex.match(line)
                   if m:
                       if m.group(1):
                           code = m.group(1)
                       else:
                           code = m.group(2)
                       currentNode.addCode("%s, %i: %s" % (currentNode.name, currentNode.lineNumber, code), newLine = True)
                       continue

        except IOError:
            raise QiError("failed to open or read file '%s'" % currentNode.name)

        timestamp = 0
        for node in self.depthFirstSearch(sourceNode):
            if node.timestamp > timestamp:
                timestamp = node.timestamp
        sourceNode.timestamp = timestamp 
  
        return sourceNode 

    # Parse the qi code embedded in the specified source and create actions
    def parse(self, source, isScanAction = False):
        sourceNode = self.scan(source)

        if sourceNode.actions != None and not isScanAction:
            return sourceNode.actions

        code = self.initializationCode[:]
        self.getCode(sourceNode, set([sourceNode]), code)
        code.extend(self.finalizationCode)

        if isScanAction:
            print "*" * 30 + " " + source + "*" * 30
            for line in code: print line
  
        if sourceNode.actions != None:
            return sourceNode.actions

        if self.options.verbose:
            print self.getTime() + " Parsing %s" % source

        ifStack = []
        currentAction = None
        currentIdentation = None 
        sourceNode.actions = []
        symbolTable = self.symbolTable.copy()
        configTable = readFromConfigFile(self.rootDir)
        if configTable == None:
            raise QiError("unable to parse the default configuration fle '%s'" % getConfigFileName(self.rootDir))
        for variable in configTable.iterkeys():
            symbolTable[variable] = configTable[variable] 
        symbolTable["source"] = source

        for line in code:
            m = self.lineInfoRegex.match(line)
            assert(m)
            fileName = m.group(1)
            lineNumber = int(m.group(2))
            line = m.group(3)

            # Bypass an empty line
            if line.strip() == "":
                currentAction = None
                continue
            
            m = self.ifRegex.match(line)
            if m:
                if m.group(1):
                    variable = m.group(2)
                    if not symbolTable.has_key(variable):
                        raise QiError("variable '%s' has not been defined" % variable, fileName, lineNumber)
                    lhs = symbolTable[variable]
                else:
                    lhs = m.group(2)
                operator = m.group(3)
                rhs = self.translate(m.group(4).strip(), symbolTable, fileName, lineNumber)
                if operator == "=" or operator == "==":
                    result = lhs == rhs 
                elif operator == "!=":
                    result = lhs != rhs 
                elif operator == "~" or operator == "~=":
                    result = bool(re.compile(rhs).search(lhs))
                else:
                    result = not bool(re.compile(rhs).search(lhs))
                ifStack.append(result)
                continue

            m = self.ifdefRegex.match(line)
            if m:
                variable = m.group(2)
                if m.group(1):
                    ifStack.append(not symbolTable.has_key(variable))
                else:
                    ifStack.append(symbolTable.has_key(variable))
                continue

            if self.elseRegex.match(line):
                if not ifStack:
                    raise QiError("'else' without corresponding 'if'", fileName, lineNumber)
                ifStack[-1] = not ifStack[-1]
                continue

            if self.endifRegex.match(line):
                if not ifStack:
                    raise QiError("'endif' without corresponding 'if'", fileName, lineNumber)
                ifStack.pop()
                continue

            if ifStack:
                skip = False
                for branch in ifStack:
                    if not branch:
                        skip = True
                        break

                if skip:
                    continue

            m = self.ruleRegex.match(line)
            if m:
                if m.group(1):
                    raise QiError("indentation is not allowed when specifying rules", fileName, lineNumber)
                if m.group(2) in self.ReservedActions:
                    raise QiError("'%s' is a reserved action name" % m.group(2), fileName, lineNumber)

                actionName = m.group(2)
                if m.group(3):
                    targets = self.translate(m.group(3)[1:-1], symbolTable, fileName, lineNumber)
                else:
                    targets = ""
                dependents = self.translate(m.group(4), symbolTable, fileName, lineNumber, defer = True)
                currentAction = self.findActionNode(actionName, source)
                if currentAction:
                    if targets != "" and currentAction.targets != targets:
                        raise QiError("action '%s' is already defined and assigned a different target name" % actionName, targets, lineNumber)
                    currentAction.dependents += " " + dependents
                else:
                    currentAction = self.addActionNode(actionName, source, targets, fileName, lineNumber)
                    currentAction.dependents = dependents 
                    if m.group(2) not in sourceNode.actions:
                        sourceNode.actions.append(m.group(2))
                currentIdentation = None
                continue

            # Check indentation
            if currentAction:
                m = self.indentationRegex.match(line)
                assert(m)
                if not m.group(1):
                    currentAction = None
                else:
                    if currentIdentation != None and m.group(1) != currentIdentation:
                        raise QiError("indentation has changed from previous lines", fileName, lineNumber) 
                    currentIdentation = m.group(1)

            m = self.assignmentRegex.match(line)
            if m:
                space = m.group(1)
                variable = m.group(2)
                operator = m.group(3)
                rhs = m.group(4).strip()  # Important: ignore leading and trailing whitespaces
                if len(rhs) >2 and rhs[0] == '"' and rhs[-1] == '"':
                    rhs = rhs[1:-1]       # Strip the outmost double quotes
                if currentAction:
                    if symbolTable.has_key(variable):
                        raise QiError("a variable that exists in the source scope cannot be reassigned in a rule", fileName, lineNumber)

                    rhs = self.translate(rhs, symbolTable, fileName, lineNumber, defer = True)
                    currentAction.commands.append((self.COMMAND_ASSIGNMENT, fileName, lineNumber, 
                                                (variable, operator, rhs)))
                else:
                    if operator == "=" or operator == ":=":
                        symbolTable[variable] = self.translate(rhs, symbolTable, fileName, lineNumber, defer = operator == ":=")
                    else:
                        rhs = self.translate(rhs, symbolTable, fileName, lineNumber)
                        if symbolTable.has_key(variable):
                            symbolTable[variable] += " " + rhs
                        else:
                            symbolTable[variable] = rhs 
                continue

            m = self.functionCallRegex.match(line.strip())
            if m:
                args = self.translate(m.group(2), symbolTable, fileName, lineNumber)
                if currentAction:
                    currentAction.commands.append((self.COMMAND_FUNCTIONCALL, fileName, lineNumber, (m.group(1), args)))
                else:
                    self.callFunction(m.group(1), args, symbolTable, fileName, lineNumber)
                continue

            # Anything else is treated as a command
            if not currentAction:
                raise QiError("syntax error: '%s'" % line, fileName, lineNumber)

            command = line.strip()  # Important: ignore leading and trailing whitespaces
            command = self.translate(command, symbolTable, fileName, lineNumber, defer = True)
            currentAction.commands.append((self.COMMAND_EXTERNAL, fileName, lineNumber, 
                                         command))
        return sourceNode.actions

    def inspect(self, action, source, toBeUpdated, isParseAction = False):
        if self.options.verbose:
            print self.getTime() + " Inspecting %s(%s)" % (action, source)
        actionNode = self.findActionNode(action, source)
        if not actionNode:
            sourceNode = self.findFileNode(source)
            if not sourceNode:
                self.parse(source)
                sourceNode = self.findFileNode(source, mustExist = True)
            actionNode = self.findActionNode(action, source, mustExist = True)
        else:
            sourceNode = self.findFileNode(source, mustExist = True)

        if not actionNode.isDependencyResolved:
            self.resolveDependency(actionNode, sourceNode)
       
        if isParseAction:
            print "%s(%s): " % (action, actionNode.targets),
            for child in actionNode.children:
                print child.name,
            print
            for command in actionNode.commands:
                if command[0] == self.COMMAND_ASSIGNMENT:
                    print "\t%s %s%s" % (command[3][0], command[3][1], command[3][2])
                elif command[0] == self.COMMAND_FUNCTIONCALL:
                    print "\t$(%s %s)" % (command[3][0], command[3][1])
                elif command[0] == self.COMMAND_EXTERNAL:
                    print "\t" + (command[3])
            return

        for node in self.depthFirstSearch(actionNode, checkCycle = True):
            if node.updateOrder >= 0: continue
            timestamp = 0
            updateOrder = -1
            for child in node.children:
                if not child.isFile and child.updateOrder > updateOrder:
                    updateOrder = child.updateOrder 
                if child.timestamp > timestamp:
                    timestamp = child.timestamp
            if updateOrder == -1:
                if timestamp > node.timestamp or node.timestamp == 0:
                    node.updateOrder = 0
            else:
                node.updateOrder = updateOrder + 1
            if node.updateOrder == -1 and self.options.force:
                node.updateOrder = 0 
            if node.updateOrder >= 0:
                if self.options.verbose:
                    print self.getTime() + " Schedule %s at update level %d" % (node.name, node.updateOrder)
                while len(toBeUpdated) <= node.updateOrder:
                    toBeUpdated.append([])
                toBeUpdated[node.updateOrder].append(node)

        return 

    def update(self, toBeUpdated):
        self.taskQueue = Queue.Queue()
        self.outputLock = threading.Lock()

        threads = []
        for i in range(self.options.jobs):
            thread = threading.Thread(target = self.parallelExecute)
            thread.setDaemon(True)
            thread.start()
            threads.append(thread)
            thread.id = i

        for level in range(len(toBeUpdated)):
            for action in toBeUpdated[level]:
                assert(action.updateOrder == level)
                symbols = {}
                for child in self.depthFirstSearch(action):
                    self.execute(child, symbols, types = self.COMMAND_ASSIGNMENT)
                self.taskQueue.put((action, symbols, self.COMMAND_EXTERNAL | self.COMMAND_FUNCTIONCALL))

            self.taskQueue.join()

        for thread in threads:
            self.taskQueue.put("STOP")
        self.taskQueue.join()
        for thread in threads:
            thread.join()

        failures = 0
        failedActions = {}
        for level in range(len(toBeUpdated)):
            for action in toBeUpdated[level]:
                if hasattr(action, "hasFailed"):
                    failures += 1 
                    actionName, sourceName = self.splitActionSources(action.name)
                    if not failedActions.has_key(sourceName):
                        failedActions[sourceName] = []
                    failedActions[sourceName].append(actionName)

        if self.options.summary:
            if failures:
                print "Failed to update the following %d actions:" % failures
                for source in failedActions.iterkeys():
                    print source + ": " + " ".join(failedActions[source])
            else:
                print "No failed actions."

        else: 
            if failures:
                print "Failed to update %d action(s)." % failures

        return failures

    def parallelExecute(self):
        isTerminating = False
        while True:
            task = self.taskQueue.get()
            if isinstance(task, basestring):
                self.taskQueue.task_done()
                return 
            if isTerminating:
                self.taskQueue.task_done()
                continue

            try:
                returnCode = self.execute(task[0], task[1], task[2])
                if returnCode < 0:
                    errorMessage = "command was terminated by signal %d" % -returnCode
                elif returnCode > 0:
                    errorMessage = "command execution returned %d" % returnCode
            except QiError, error:
                if error.fileName:
                    errorMessage = "%s (%s:%d)" % (error.message, error.fileName, error.lineNumber) 
                else:
                    errorMessage = error.message
                returnCode = 1
            except:
                errorMessage = "exception '%s' was raised updating action '%s'" % (sys.exc_info()[0], task[0].name)
                returnCode = 1

            self.taskQueue.task_done()

            if returnCode != 0: 
                self.outputLock.acquire()
                print "Error: %s." % errorMessage
                self.outputLock.release()
                if returnCode == -2:
                    isTerminating = True
                else:
                    task[0].hasFailed = True
                    isTerminating = not self.options.keepGoing
                    isTerminating = not self.options.keepGoing

    def resolveDependency(self, actionNode, sourceNode):
        actionNode.isDependencyResolved = True
        dependents = self.translate(actionNode.dependents, {}, actionNode.fileName, actionNode.lineNumber).strip()
        if self.options.verbose:
            if dependents: 
                print self.getTime() + " %s is dependent on %s" % (actionNode.name, dependents.strip()) 
            else:
                print self.getTime() + " %s has no dependency" % actionNode.name
                return 
        dependents = self.split(dependents)

        actionNode.dependents = None
        for dependent in dependents:
            action, sources = self.splitActionSources(dependent)
            if not action:
                node = self.findActionNode(dependent, sourceNode.name)
                if node:
                    if not node.isDependencyResolved: 
                        self.resolveDependency(node, sourceNode)
                else:
                    node = self.findFileNode(dependent)
                    if not node:
                        node = self.addFileNode(dependent)
                self.setDependency(actionNode, node)
            else:
                if not sources:
                    continue
                if sources.find("(") != -1 or sources.find("$") != -1:
                    raise QiError("'%s' can't be evaluated." % sources, actionNode.fileName, actionNode.lineNumber)
                for source in sources.split():
                    self.parse(source)
                    node = self.findActionNode(action, source, mustExist = True)
                    if not node.isDependencyResolved: 
                        self.resolveDependency(node, self.findFileNode(source, mustExist = True))
                    self.setDependency(actionNode, node)    

    def execute(self, node, symbols, types):
        for command in node.commands:
            if command[0] & types == 0: continue
            fileName = command[1]
            lineNumber = command[2]
            if command[0] == self.COMMAND_ASSIGNMENT:
                variable = command[3][0] 
                operator = command[3][1] 
                rhs = self.translate(command[3][2], symbols, fileName, lineNumber) 
                if operator == "=":
                    symbols[variable] = rhs
                elif symbols.has_key(variable):
                    symbols[variable] += " " + rhs
                else:
                    symbols[variable] = rhs
            elif command[0] == self.COMMAND_FUNCTIONCALL:
                function = command[3][0]
                args = self.translate(command[3][1], symbols, fileName, lineNumber)
                self.callFunction(function, args, symbols, fileName, lineNumber)
            else:
                realCommand = self.translate(command[3], symbols, fileName, lineNumber)
                if not realCommand:
                    continue
                if realCommand[0] == '@':
                    echo = False 
                    realCommand = realCommand[1:]
                else:
                    echo = True 
                output, returnCode = "", 0
                if not self.options.justPrint:
                    output, returnCode = self.shell(realCommand)
                output = output.rstrip()
                if self.options.jobs == 1:
                    if echo and not self.options.silent:
                        print realCommand
                    if output and not self.options.silent:
                        print output
                    if self.options.verbose:
                        print self.getTime() + " Updated %s at level %d" % (node.name, node.updateOrder) 
                else:
                    self.outputLock.acquire()
                    if echo and not self.options.silent:
                        print "[%d] %s" % (threading.currentThread().id, realCommand)
                    if output and not self.options.silent:
                        print output
                    if self.options.verbose:
                        print "[%d] " % threading.currentThread().id + self.getTime() + " Updated %s at level %d" % (node.name, node.updateOrder) 
                    self.outputLock.release()
                if returnCode != 0:
                    return returnCode
        return 0

    def shell(self, command):
        if platform.system() == "Windows":
            commandList = command.split()
        else:
            commandList = shlex.split(command)
        process = subprocess.Popen(commandList, shell = False, stdout = subprocess.PIPE, stderr = subprocess.STDOUT)
        output = process.communicate()[0]
        return output, process.returncode

    def getCode(self, startingNode, visitedNodes, code):
        for line in startingNode.code:
            if isinstance(line, basestring):
                code.append(line)
            else:
                node = line
                if node not in visitedNodes: 
                    visitedNodes.add(node)
                    self.getCode(node, visitedNodes, code)
      
    def depthFirstSearch(self, startingNode, checkCycle = False):
        visitedNodes = set() 
        nodeStack = [startingNode]
        startingNode.nextChild = 0

        while nodeStack:
            node = nodeStack[-1]
            node.isVisited = True
            while node.nextChild < len(node.children):
                child = node.children[node.nextChild]
                if child.isFile != startingNode.isFile:
                    node.nextChild += 1
                    continue
                if child not in visitedNodes:
                    if not child.children:
                        yield child 
                        visitedNodes.add(child)
                    else:
                        if checkCycle and child in nodeStack:
                            cycle = ""
                            for node in nodeStack[nodeStack.index(child):]:
                                cycle += "%s -> " % node.name
                            cycle += child.name 
                            raise QiError("circular dependency detected: %s" % cycle)
                        nodeStack.append(child)
                        child.nextChild = 0
                        child.isVisited = False
                        break
                node.nextChild += 1
            if node.nextChild >= len(node.children):
                yield node
                visitedNodes.add(node)
                nodeStack.pop()

        for node in visitedNodes:
            node.isVisited = False 

    def translate(self, input, symbolTable, fileName, lineNumber, defer = False):
        outputStack = [ ["", 0] ]
        pos = 0
        while True:
            m = self.stringParserRegex.search(input[pos:])
            if not m:
                if len(outputStack) != 1:
                    raise QiError("unclosed left parenthesis", fileName, lineNumber)
                else:
                    return outputStack[0][0] + input[pos:]
            outputStack[-1][0] += input[pos:pos + m.start()]
            pos += m.end()
            if m.lastgroup == "var1":
                name = m.group(0)[1:]
                if symbolTable.has_key(name):
                    outputStack[-1][0] += symbolTable[name]
                else:
                    outputStack[-1][0] += m.group(0)
            elif m.lastgroup == "var2":
                name = m.group(0)[2:-1]
                if symbolTable.has_key(name):
                    outputStack[-1][0] += symbolTable[name]
                else:
                    outputStack[-1][0] += m.group(0)
            elif m.lastgroup == "funct":
                outputStack.append([m.group(0), 1])
            elif m.lastgroup == "action":
                outputStack.append([m.group(0), 1])
            elif m.lastgroup == "dollar":
                outputStack[-1][0] += "$"
            elif m.lastgroup == "lparen":
                outputStack[-1][0] += "("
                outputStack[-1][1] += 1
            elif m.lastgroup == "rparen":
                outputStack[-1][0] += ")"
                outputStack[-1][1] -= 1
                if outputStack[-1][1] == 0 and len(outputStack) > 1:
                    m = self.functionCallRegex.match(outputStack[-1][0])
                    if m:
                        if defer:
                            outputStack[-2][0] += outputStack[-1][0]
                        else:
                            outputStack[-2][0] += self.callFunction(m.group(1), m.group(2), symbolTable, fileName, lineNumber)
                        outputStack.pop()
                        continue
                    if defer:
                        outputStack[-2][0] += outputStack[-1][0]
                    else:
                        action, sources = self.splitActionSources(outputStack[-1][0])
                        outputStack[-2][0] += self.expandAction(action, sources, symbolTable, fileName, lineNumber)
                    outputStack.pop()

    def callFunction(self, function, args, symbolTable, fileName, lineNumber):
        if function == "shell":
            output, returnCode = self.shell(args)
            return output.strip()
        if function == "eval":
            return self.translate(args, symbolTable, fileName, lineNumber)
        if function == "print":
            print args
            return ""       
        if function == "reverse":
            results = self.split(args)
            results.reverse()
            return " ".join(results)
        if function == "match":
            args = args.split()
            if len(args) <= 1:
                return ""
            regex = re.compile(args[0])
            results = []
            for arg in args[1:]:
                m = regex.search(arg)
                if m:
                    results.append(m.group(1))
            return " ".join(results)
        if function == "add_prefix":
            args = args.split() 
            return " ".join([ args[0] + args for arg in arg[1:0]])
        if function == "add_suffix":
            args = args.split() 
            return " ".join([ arg + args[0] for arg in arg[1:0]])
        if function == "list":
            sources = [] 
            for dir in self.split(args):
                for source in self.sourceHeaderMap.keys():
                    if source.startswith(dir):
                        sources.append(source)
            return " ".join(sources)
        if function == "get_headers":
            headers = []
            for source in self.split(args):
                if source in self.sourceHeaderMap.keys():
                    self.scan(source)
                    headers.extend(self.sourceHeaderMap[source])
            return " ".join(headers)
        if function == "get_sources":
            sources = [] 
            for header in self.split(args):
                if header in self.headerSourceMap.keys():
                    sources.extend(self.headerSourceMap[header])
                else:
                    base = os.path.splitext(header)[0]
                    if base in self.baseSourceMap.iterkeys():
                        sources.append(self.baseSourceMap[base])
            return " ".join(sources)
        if function == "join":
            paths = args.split()
            if not paths:
                return "" 
            results = [] 
            for path in paths[1:]:
                results.append(join(paths[0], path))
            return " ".join(results)
        if function == "remove":
            args = self.split(args)
            if not args:
                return ""
            if len(args[0]) > 2 and args[0][0] == "(" and args[0][-1] == ")":
                first = args[0][1:-1].split()
            else:
                first = [args[0]]
            for arg in args[1:]:
                if arg in first:
                    first.remove(arg)
            return " ".join(first)
        if function == "exist":
            args = self.split(args)
            for arg in args:
                if not os.path.exists(join(self.rootDir, arg)):
                    return "0"
            return "1"
        if function == "compile_depends":
            results = set()
            for source in self.split(args):
                sourceNode = self.scan(source)
                for node in self.depthFirstSearch(sourceNode):
                    if node != sourceNode and node.name not in results:
                        results.add(node.name)
            return " ".join(list(results))
        if function == "link_depends":
            results = []
            visistedHeaders = set() 
            self.getLinkDepends(args.strip(), results, visistedHeaders)
            return " ".join(results)
        if function == "file_name":
            return fileName
        if function == "line_number":
            return str(lineNumber)
        if function == "dir":
            return os.path.dirname(args)
        if function == "mkdir":
            try:
                os.makedirs(args)
            except:
                pass
            return True
        if function == "platform":
            return platform.system() 
        if self.userDefinedFunctions.has_key(function):
            try:
                udf = self.userDefinedFunctions[function]
                if udf.func_code.co_argcount == 2:
                    return udf(self, args)
                else:
                    return udf(args) 
            except:
                raise QiError("error when calling the user-defined function '%s'" % function, fileName, lineNumber)
                return ""
 
        raise QiError("function '%s' is not implemented" % function, fileName, lineNumber)
         
    def expandAction(self, action, args, symbolTable, fileName, lineNumber):
        args = self.translate(args, symbolTable, fileName, lineNumber)
        sources = args.split()
        results = [] 
        for source in sources:
            self.parse(source)
            node = self.findActionNode(action, source)
            if node:
                results.append(node.targets)
        return " ".join(results) 
        
    def getLinkDepends(self, sourceFile, results, visitedHeaders):
        source = self.findFileNode(sourceFile)
        if not source:
            source = self.scan(sourceFile)
        for node in self.depthFirstSearch(source):
            if node != source and node not in visitedHeaders and \
                node.name in self.headerSourceMap.iterkeys():
                visitedHeaders.add(node)
                for matched in self.headerSourceMap[node.name]:
                    if not matched in results:
                        self.getLinkDepends(matched, results, visitedHeaders)
                for matched in self.headerSourceMap[node.name]:
                    if not matched in results:
                        results.append(matched)
                    
        if sourceFile not in results:
            results.append(sourceFile)

    def split(self, input):
        results = []
        start = 0
        while True:
            pos = input.find("(", start)
            if pos == -1:
                results.extend(input[start:].split())
                return results
            end = pos + 1
            level = 1
            while end < len(input):
                if input[end] == '(':
                    level += 1
                elif input[end] == ')':
                    level -= 1
                    if level == 0:
                        break;
                end += 1
            results.extend(input[start:pos + 1].split())
            if level == 0:
                results[-1] += input[pos + 1 : end + 1]
                start = end + 1
            else:
                results[-1] += input[pos + 1 : end + 1]
                return results

    def splitActionSources(self, actionSources):
        m = self.actionSourcesRegex.search(actionSources)
        if m:
            return m.group(1), m.group(2)
        else:
            return None, None

    def getNode(self, name):
        if name not in self.nodes.iterkeys():
            return None
        return self.nodes[name]
 
    def addNode(self, name, targets, isFile, fileName, lineNumber):
        if name in self.nodes.iterkeys():
            return self.nodes[name]

        node = Node(name, targets, isFile, fileName, lineNumber)
        self.nodes[name] = node
        node.timestamp = 0
        if targets: 
            for target in targets.split():
                targetName = join(self.rootDir, target)
                if os.path.exists(targetName):
                    if node.timestamp == 0 or node.timestamp > os.path.getmtime(targetName):
                        node.timestamp = os.path.getmtime(targetName)
                else:
                    node.timestamp = 0
                    break
        return node 

    def addFileNode(self, name, mayNotExist = False):
        node = self.addNode(name, name, isFile = True, fileName = None, lineNumber = None)
        node.scanned = False
        node.actions = None
        node.code = [] 
        if node.timestamp == 0:
            if mayNotExist:
                node.scanned = True
            else:
                raise QiError("The file or directory '%s' doesn't exist" % name)
        return node

    def addActionNode(self, actionName, sourceName, targets, fileName, lineNumber):
        name = "%s(%s)" % (actionName, sourceName)
        node = self.addNode(name, targets, isFile = False, fileName = fileName, lineNumber = lineNumber)
        node.commands = []
        node.isDependencyResolved = False
        node.updateOrder = -1 
        return node

    def setDependency(self, dependent, dependee):
        if dependee not in dependent.children:
            dependent.children.append(dependee)
            return True
        return False

    def findFileNode(self, name, mustExist = False):
        if self.nodes.has_key(name):
            node = self.nodes[name]
            if not node.isFile:
                raise QiError("'%s' is expected to be a file, not a action" % name)
            return node
        if mustExist:
            raise QiError("A file node named '%s' cannot be found" % name)
        return None

    def findActionNode(self, actionName, sourceName, mustExist = False):
        name = "%s(%s)" % (actionName, sourceName)
        if self.nodes.has_key(name):
            node = self.nodes[name]
            if node.isFile:
                raise QiError("'%s' is expected to be a action, not a file" % name)
            return node
        if mustExist:
            sourceNode = self.findFileNode(sourceName)
            if not sourceNode:
                raise QiError("file '%s' has not been parsed" % m.group(2))
            else:
                raise QiError("there is no action named '%s' within '%s'" % (actionName, sourceName))
        return None

    def getTime(self):
        now = time.localtime(time.time())
        return  time.strftime("[%m-%d-%Y %H:%M:%S]")

def getStandardName(rootDir, path):
    path = normalizePath(os.path.realpath(path))
    if path.startswith(rootDir):
        return path[len(rootDir) + 1:]
    else:
        return path 

def join(rootDir, path):
    return os.path.join(rootDir, path)

def normalizePath(path):
    if platform.system() == "Windows":
        return path.replace("/", "\\")
    else:
        return path.replace("\\", "/")

def getConfigFileName(rootDir):
    return join(rootDir, ".qi-%s-%s.conf" % (platform.node(), getpass.getuser()))

def isFileReadable(rootDir, fileName):
    fullName = join(rootDir, file)
    try:
        file = open(fullName, "r")
        file.close()
    except IOError:
        return False
    return True
     
def readFromConfigFile(rootDir):
    try:
        file = open(getConfigFileName(rootDir))
        variableTable = {}
        keyValueRegex = re.compile(r"(\w+)\s*=\s*(([^\r\n]*))")
        for line in file.readlines():
            m = keyValueRegex.match(line)
            if not m:
                file.close()
                return None
            variableTable[m.group(1)] = m.group(2)
        file.close()
        return variableTable
    except IOError:
        return {} 

def writeToConfigFile(rootDir, variableTable):
    try:
        fileName = getConfigFileName(rootDir)
        file = open(fileName, "w")
        for variable in variableTable.iterkeys():
            file.write("%s = %s\n" % (variable, variableTable[variable]))
        file.close()
    except IOError:
        print "Error: failed to open or write to the default config file '%s'" % fileName
        return 1 
    return 0 

def main():
    parser = optparse.OptionParser("usage: %prog [options] [action [source files]]")
    parser.add_option("-j", "--jobs", dest="jobs", type="int",
                      action="store", default=1,
                      help="number of threads to execute external commands")
    parser.add_option("-n", "--just-print", dest="justPrint",
                      action="store_true", default=False,
                      help="don't run commands; just print them")
    parser.add_option("-k", "--keep-going", dest="keepGoing",
                      action="store_true", default=False,
                      help="don't stop on errors; keep going")
    parser.add_option("-f", "--force", dest="force",
                      action="store_true", default=False,
                      help="rebuild actions even if they are up to date")
    parser.add_option("-s", "--silent", dest="silent",
                      action="store_true", default=False,
                      help="don't echo commands when executing them")
    parser.add_option("-S", "--summary", dest="summary",
                      action="store_true", default=False,
                      help="print a summary of actions that failed")
    parser.add_option("-a", "--all", dest="all",
                      action="store_true", default=False,
                      help="process all specified source files even "
                           "if they are not registered")
    parser.add_option("-V", "--version", dest="printVersion",
                      action="store_true", default=False,
                      help="print version information")
    parser.add_option("-v", "--verbose", dest="verbose",
                      action="store_true", default=False,
                      help="print detailed information about what is being done")

    (options, args) = parser.parse_args()

    if options.printVersion:
        print """Qi-Make: A Link-Smart Build System for C/C++ 
Version 1.0 
Written by Gilbert (Gang) Chen
http://code.google.com/p/qi-make"""
        return 0
              
    # rootDir: top level directory (where the dependency file is) 
    # workDir: the current working directory
    # buildDir: where all objects and executables are located. 

    # Set 'workDir'
    workDir = os.getcwd()
    if len(args) >= 2: 
        source = os.path.abspath(args[1])
        if os.path.isdir(source):
            workDir = source
        else:
            workDir = os.path.dirname(source)

    # Handle 'init'
    if args and args[0] == "init":
        if os.path.exists(join(workDir, PROJECT_FILE)):
            print "Error: the default project file '%s' already exists." % PROJECT_FILE
            return 1
        try: 
            projectFile = open(join(workDir, PROJECT_FILE), "w")
            projectFile.write(SOURCE_FILES_SECTION + "\n")
            projectFile.write(INITIALIZATION_CODE_SECTION + "\n")
            projectFile.write(FINALIZATION_CODE_SECTION + "\n")
            projectFile.write(USER_DEFINED_FUNCTIONS_SECTION + "\n")
            projectFile.close()
        except IOError:
            print "Error: failed to create the default project file '%s'." % PROJECT_FILE
            return 1
        return 0

     # Set 'rootDir'
    rootDir = normalizePath(workDir)
    while not os.path.exists(join(rootDir, PROJECT_FILE)):
        parentDir = os.path.dirname(rootDir)
        if parentDir == rootDir:
            print "Error: the default projet file '%s' is not found." % PROJECT_FILE
            return 1 
        rootDir = parentDir
    workDir = getStandardName(rootDir, workDir)

    try:
        sourceHeaderMap = {}
        initializationCode = []
        finalizationCode = []
        projectSections = {}
        otherSections = "" 

        projectFile = open(join(rootDir, PROJECT_FILE), "r")
        lineNumber = 0 
        currentSection = ""
        for line in projectFile.readlines():
            line = line.strip("\r\n")
            if otherSections:
                otherSections += line + "\n" 
            lineNumber += 1
            if len(line) > 2 and line[0] == '[' and line[-1] == ']':
                projectSections[line] = []
                if currentSection == SOURCE_FILES_SECTION and currentSection != line:
                    otherSections += line + "\n"
                currentSection = line
                continue
            if currentSection:
                projectSections[currentSection].append((lineNumber, line))

        if not projectSections.has_key(SOURCE_FILES_SECTION):
            raise QiError("can't locate the section containing source files", PROJECT_FILE, 1)
        for lineNumber, line in projectSections[SOURCE_FILES_SECTION]:
            files = line.split()
            if files:
                sourceHeaderMap[normalizePath(files[0])] = files[1:]

        if projectSections.has_key(INITIALIZATION_CODE_SECTION):
            for lineNumber, line in projectSections[INITIALIZATION_CODE_SECTION]:
                initializationCode.append("%s, %i: %s" % (PROJECT_FILE, lineNumber, line))

        if projectSections.has_key(FINALIZATION_CODE_SECTION):
            for lineNumber, line in projectSections[FINALIZATION_CODE_SECTION]:
                finalizationCode.append("%s, %i: %s" % (PROJECT_FILE, lineNumber, line))

        userDefinedModule = imp.new_module("udm")
        if projectSections.has_key(USER_DEFINED_FUNCTIONS_SECTION) and \
                projectSections[USER_DEFINED_FUNCTIONS_SECTION]:
            userDefinedFunctions = ""
            for lineNumber, line in projectSections[USER_DEFINED_FUNCTIONS_SECTION]:
                userDefinedFunctions += line + "\n"
            try:
                exec userDefinedFunctions in userDefinedModule.__dict__
            except SyntaxError, error:
                print "%s, %d: syntax error in user-defined functions" % (PROJECT_FILE,
                      projectSections[INITIALIZATION_CODE_SECTION][0][0] + error.lineno - 1)
                print userDefinedFunctions
                return 1

        projectFile.close()

    except IOError:
        print "Error: unable to open or read the default project file '%s'." % PROJECT_FILE
        return 1
    except QiError, error:
        if error.fileName:
            print "Error at %s:%d: %s" % (error.fileName, error.lineNumber, error.message)
        else:
            print "Error: %s" % error.message
        return 1 
  
    if args and (args[0] == "set" or args[0] == "unset"):
        variableTable = readFromConfigFile(rootDir)
        if variableTable == None:
            print "Error: unable to parse the default configuration file '%s'." % getConfigFileName(rootDir)
            return 1

        if len(args) == 1:
            for variable in variableTable.iterkeys():
                print variable + " = " + variableTable[variable]
            return 0

        if len(args) == 2:
            if args[0] == "set":
                variableTable[args[1]] = ""
                print args[1] + " = "
            else:
                if variableTable.has_key(args[1]):
                    del variableTable[args[1]]
            return writeToConfigFile(rootDir, variableTable)

        if len(args) >= 3:
            if args[0] == "set":
                print "%s = %s" % (args[1], args[2])
                variableTable[args[1]] = args[2]
            else:
                for variable in args[1:]:
                    if variableTable.has_key(variable):
                        del variableTable[variable]
            return writeToConfigFile(rootDir, variableTable)

    if args and (args[0] == "add" or args[0] == "delete"):
        if len(args) < 2:
            print "Error: a source file must be specified."
            return 1
  
        source = getStandardName(rootDir, os.path.abspath(args[1]))
        header = None
        if len(args) >= 3:
            header = getStandardName(rootDir, os.path.abspath(args[2])) 
       
        if args[0] == "add":
            try:
                open(os.path.abspath(args[1]), "r").close()
            except IOError:
                print "Error: the specified source file '%s' cannot be opened." % source
                return 1

            if header:
                try:
                    open(os.path.abspath(args[2]), "r").close()
                except IOError:
                    print "Error: the specified header file '%s' cannot be opened." % header 
                    return 1

                if source in sourceHeaderMap.iterkeys():
                    if not header in sourceHeaderMap[source]:
                        sourceHeaderMap[source].append(header)
                else:
                    sourceHeaderMap[source] = [header]
            else:
                if source not in sourceHeaderMap.iterkeys():
                    sourceHeaderMap[source] = []
        else:
            if not source in sourceHeaderMap.iterkeys():
                print "Error: no source file named '%s' in the default project file '%s'." % (source, PROJECT_FILE)
                return 1
            if header:
                if header in sourceHeaderMap[source]:
                    sourceHeaderMap[source].remove(header)
            else:
                del sourceHeaderMap[source]

        try:
            projectFile = open(join(rootDir, PROJECT_FILE), "w") 
            sources = sourceHeaderMap.keys()
            sources.sort()
            projectFile.write(SOURCE_FILES_SECTION + "\n")
            for source in sources:
                projectFile.write("%s %s\n" % (source, " ".join(sourceHeaderMap[source])))
            projectFile.write(otherSections)
            projectFile.close()
        except IOError:
            print "Erorr: failed to open or write to the default project file '%s'." % PROJECT_FILE
            return 1
        return 0

    if args and args[0] == "list":
        if len(args) == 1:
            file = workDir 
        else:
            file = getStandardName(rootDir, os.path.abspath(args[1]))
        sources = sourceHeaderMap.keys()
        sources.sort()
        for source in sources:
            if source.startswith(file):
                print source + ": " + " ".join(sourceHeaderMap[source])
        return 0

    builder = Builder(rootDir, sourceHeaderMap,
                      initializationCode, finalizationCode,
                      userDefinedModule, options)

    try:
        if len(args) <= 1:
            # Only one argument is specified which is the action.
            sources = [source for source in sourceHeaderMap.iterkeys() if source.startswith(workDir)]
            if not sources:
                print "Error: no registered source files under '%s'." % workDir
                return 1
        else:
            sources = []
            for arg in args[1:]:
                fullPath = os.path.abspath(arg)
                path = getStandardName(rootDir, fullPath)
                if os.path.isfile(fullPath):
                    if path not in sourceHeaderMap.keys() and not options.all:
                        print "Error: '%s' has not been registered." % path
                        return 1
                    if path not in sources:
                        sources.append(path)
                    continue 
                   
                if os.path.isdir(fullPath):
                    for source in sourceHeaderMap.iterkeys():
                        if source.startswith(path) and source not in sources:
                            sources.append(source)

        if not sources:
            print "Error: no valid source file specified."
            return 1
        toBeUpdated = []
        isActionDefined = False 
        for source in sources:
            if args:
                # An action is specified; update that action
                if args[0] == "scan":
                    isActionDefined = True
                    actions = builder.parse(source, isScanAction = True)
                elif args[0] == "parse":
                    isActionDefined = True
                    actions = builder.parse(source)
                    print "*" * 30 + " " + source + "*" * 30
                    for action in actions:
                        builder.inspect(action, source, toBeUpdated, isParseAction = True)
                else:
                    if len(args) == 1:
                        # No source file specified ('sources' is collected from
                        # the current working directory); only update the
                        # specified action if that action is defined for the source
                        actions = builder.parse(source)
                        if args[0] in actions:
                            isActionDefined = True
                            builder.inspect(args[0], source, toBeUpdated)
                    else:
                        # Update the action anyway (will error out inside 'inspect'
                        # if the action is not defined)
                        isActionDefined = True
                        builder.inspect(args[0], source, toBeUpdated) 
            else:
                # No action specified; update the first action defined in each source
                isActionDefined = True
                actions = builder.parse(source)
                if actions:
                    builder.inspect(actions[0], source, toBeUpdated)

        if not isActionDefined:
            print "Error: the action '%s' is not defined." % args[0] 
            return 2 

        if toBeUpdated:
            os.chdir(rootDir)
            if builder.update(toBeUpdated):
                return 3 
            else:
                return 0 
        else:
            return 0

    except QiError, error:
        if error.fileName:
            print "Error at %s:%d: %s" % (error.fileName, error.lineNumber, error.message)
        else:
            print "Error: %s" % error.message
        return 2

    except KeyboardInterrupt:
        return 2
 
if __name__ == "__main__":
    sys.exit(main())
