import * as path from 'path';
import * as fs from 'fs';
import { workspace, ExtensionContext, window, commands, Uri, Terminal } from 'vscode';

import {
  LanguageClient,
  LanguageClientOptions,
  ServerOptions,
} from 'vscode-languageclient/node';

let client: LanguageClient;
let outputChannel = window.createOutputChannel('Neutron Language Server');
let runTerminal: Terminal | undefined;

function getNeutronPath(): string {
    const config = workspace.getConfiguration('neutron');
    let neutronPath = config.get<string>('path');
    
    if (neutronPath && fs.existsSync(neutronPath)) {
        return neutronPath;
    }
    
    // Try workspace build folder
    if (workspace.workspaceFolders && workspace.workspaceFolders.length > 0) {
        const isWindows = process.platform === 'win32';
        const executableName = isWindows ? 'neutron.exe' : 'neutron';
        
        for (const folder of workspace.workspaceFolders) {
            const buildPath = path.join(folder.uri.fsPath, 'build', executableName);
            if (fs.existsSync(buildPath)) {
                return buildPath;
            }
        }
    }
    
    // Fallback to global PATH
    return process.platform === 'win32' ? 'neutron.exe' : 'neutron';
}

export function activate(context: ExtensionContext) {
  outputChannel.appendLine('[Extension] Activating Neutron extension...');
  outputChannel.appendLine(`[Extension] Extension path: ${context.extensionPath}`);
  
  // Get the server path from configuration
  const config = workspace.getConfiguration('neutron');
  let serverCommand = config.get<string>('lsp.path');

  outputChannel.appendLine(`[Extension] Config path: ${serverCommand || '(not set)'}`);

  // If path is not set in config, try to resolve it automatically
  if (!serverCommand) {
    // 1. Check local build/ folder (Dev environment convenience)
    if (workspace.workspaceFolders && workspace.workspaceFolders.length > 0) {
        const isWindows = process.platform === 'win32';
        const executableName = isWindows ? 'neutron-lsp.exe' : 'neutron-lsp';
        
        // Try each workspace folder
        for (const folder of workspace.workspaceFolders) {
            // Check build directory first
            const buildPath = path.join(folder.uri.fsPath, 'build', executableName);
            outputChannel.appendLine(`[Extension] Checking: ${buildPath}`);
            
            if (fs.existsSync(buildPath)) {
                serverCommand = buildPath;
                outputChannel.appendLine(`[Extension] Found LSP at: ${buildPath}`);
                break;
            }
            
            // Check build/Release directory (Windows MSVC)
            if (isWindows) {
                const releasePath = path.join(folder.uri.fsPath, 'build', 'Release', executableName);
                outputChannel.appendLine(`[Extension] Checking: ${releasePath}`);
                
                if (fs.existsSync(releasePath)) {
                    serverCommand = releasePath;
                    outputChannel.appendLine(`[Extension] Found LSP at: ${releasePath}`);
                    break;
                }
            }
            
            // Check project root (after running fix-lsp script)
            const rootPath = path.join(folder.uri.fsPath, executableName);
            outputChannel.appendLine(`[Extension] Checking: ${rootPath}`);
            
            if (fs.existsSync(rootPath)) {
                serverCommand = rootPath;
                outputChannel.appendLine(`[Extension] Found LSP at: ${rootPath}`);
                break;
            }
        }
    }
    
    // 2. Check extension directory (for packaged extension)
    if (!serverCommand) {
        const isWindows = process.platform === 'win32';
        const executableName = isWindows ? 'neutron-lsp.exe' : 'neutron-lsp';
        const extensionLspPath = path.join(context.extensionPath, executableName);
        
        if (fs.existsSync(extensionLspPath)) {
            serverCommand = extensionLspPath;
            outputChannel.appendLine(`[Extension] Found LSP in extension: ${extensionLspPath}`);
        }
    }
  }

  // 3. Fallback to global PATH
  if (!serverCommand) {
      serverCommand = process.platform === 'win32' ? "neutron-lsp.exe" : "neutron-lsp";
      outputChannel.appendLine(`[Extension] Using PATH fallback: ${serverCommand}`);
  }

  outputChannel.appendLine(`[Extension] Final server command: ${serverCommand}`);

  // Verify the binary exists if it's an absolute path
  if (path.isAbsolute(serverCommand) && !fs.existsSync(serverCommand)) {
      const errorMsg = `Neutron LSP binary not found at: ${serverCommand}`;
      outputChannel.appendLine(`[Extension] ERROR: ${errorMsg}`);
      window.showErrorMessage(errorMsg);
      return;
  }

  const serverOptions: ServerOptions = {
    command: serverCommand,
    args: [],
    options: {
      env: process.env,
      shell: false
    }
  };

  const clientOptions: LanguageClientOptions = {
    documentSelector: [{ scheme: 'file', language: 'neutron' }],
    synchronize: {
      fileEvents: workspace.createFileSystemWatcher('**/*.nt')
    },
    outputChannel: outputChannel,
    traceOutputChannel: outputChannel
  };

  client = new LanguageClient(
    'neutronLanguageServer',
    'Neutron Language Server',
    serverOptions,
    clientOptions
  );

  outputChannel.appendLine(`[Extension] Starting language client...`);
  outputChannel.show(true);

  client.start().then(() => {
      outputChannel.appendLine('[Extension] LSP Client started and connected.');
  }).catch((error: Error) => {
      outputChannel.appendLine(`[Extension] Client start error: ${error.message}`);
      outputChannel.appendLine(`[Extension] Stack: ${error.stack}`);
      
      // Provide helpful error messages based on the error
      if (error.message.includes('EPIPE') || error.message.includes('ENOENT')) {
          const isWindows = process.platform === 'win32';
          const fixScript = isWindows ? 'scripts\\fix-lsp.bat' : 'scripts/fix-lsp.sh';
          
          window.showErrorMessage(
              `Neutron LSP failed to start. This is usually due to missing dependencies.`,
              'Open Fix Guide',
              'Retry'
          ).then(selection => {
              if (selection === 'Open Fix Guide') {
                  const message = isWindows 
                      ? `To fix LSP issues on Windows:\n\n1. Run ${fixScript} from the Neutron source directory\n2. Or install Visual C++ Redistributable from:\n   https://aka.ms/vs/17/release/vc_redist.x64.exe\n3. Reload VS Code`
                      : `To fix LSP issues on Linux/macOS:\n\n1. Run ${fixScript} from the Neutron source directory\n2. Or install dependencies manually:\n   sudo apt install libjsoncpp-dev libcurl4-openssl-dev\n3. Reload VS Code`;
                  
                  window.showInformationMessage(message, { modal: true });
              } else if (selection === 'Retry') {
                  commands.executeCommand('workbench.action.reloadWindow');
              }
          });
      } else {
          window.showErrorMessage(`Neutron LSP failed: ${error.message}`);
      }
  });
  
  // Register Run command
  const runFileCommand = commands.registerCommand('neutron.runFile', async (uriArg?: string) => {
      let filePath: string;
      
      if (uriArg) {
          // Called from CodeLens with URI argument
          filePath = uriArg.startsWith('file://') ? Uri.parse(uriArg).fsPath : uriArg;
      } else {
          // Called from command palette
          const editor = window.activeTextEditor;
          if (!editor) {
              window.showErrorMessage('No active Neutron file');
              return;
          }
          filePath = editor.document.uri.fsPath;
      }
      
      if (!filePath.endsWith('.nt') && !filePath.endsWith('.ntsc')) {
          window.showErrorMessage('Not a Neutron file');
          return;
      }
      
      const neutronPath = getNeutronPath();
      
      // Create or reuse terminal
      if (!runTerminal || runTerminal.exitStatus !== undefined) {
          runTerminal = window.createTerminal('Neutron');
      }
      runTerminal.show();
      runTerminal.sendText(`"${neutronPath}" "${filePath}"`);
  });
  
  // Register Format command
  const formatFileCommand = commands.registerCommand('neutron.formatFile', async (uriArg?: string) => {
      let filePath: string;
      
      if (uriArg) {
          filePath = uriArg.startsWith('file://') ? Uri.parse(uriArg).fsPath : uriArg;
      } else {
          const editor = window.activeTextEditor;
          if (!editor) {
              window.showErrorMessage('No active Neutron file');
              return;
          }
          filePath = editor.document.uri.fsPath;
      }
      
      if (!filePath.endsWith('.nt') && !filePath.endsWith('.ntsc')) {
          window.showErrorMessage('Not a Neutron file');
          return;
      }
      
      const neutronPath = getNeutronPath();
      
      // Run format command
      const { exec } = require('child_process');
      exec(`"${neutronPath}" fmt "${filePath}"`, (error: Error | null, stdout: string, stderr: string) => {
          if (error) {
              window.showErrorMessage(`Format error: ${stderr || error.message}`);
              return;
          }
          window.showInformationMessage('File formatted successfully');
          // Reload the file to show changes
          commands.executeCommand('workbench.action.files.revert');
      });
  });
  
  context.subscriptions.push(client, runFileCommand, formatFileCommand);
}

export function deactivate(): Thenable<void> | undefined {
  if (!client) {
    return undefined;
  }
  return client.stop();
}
