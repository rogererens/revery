/* print_endline ("Hello, world"); */

open Revery;
open Revery.Draw;
open Revery.Math;
open Revery.UI;
open Js_of_ocaml;
open PlaygroundLib;
open PlaygroundLib.Types;


let rootNode: ref(option(viewNode)) = ref(None);

let createNode = (nodeType) => switch(nodeType) {
| View => (new viewNode)()
| Text => Obj.magic((new textNode)(""));
/* | _ => (new viewNode)() */
};

let idToNode: Hashtbl.t(int, node) = Hashtbl.create(100);

let nodeFromId = (id: int) => {
    let item = Hashtbl.find_opt(idToNode, id);
    switch (item) {
    | Some(v) => v
    | None => failwith("Cannot find node: " ++ string_of_int(id))
    }
};

let visitUpdate = u => switch(u) {
    | RootNode(id) => {
        print_endline ("update: rootnode: " ++string_of_int(id));
        let node = nodeFromId(id);
        rootNode := Obj.magic(Some(node));
        
    }
    | NewNode(id, nodeType) => {
        print_endline ("update: newnode: " ++string_of_int(id));
        let node = createNode(nodeType);
        Hashtbl.add(idToNode, id, node);
    }
    | SetStyle(id, style) => {
        print_endline ("update: setstyle: " ++string_of_int(id));
        let node = nodeFromId(id);
        node#setStyle(style);
    }
    | AddChild(parentId, childId) => {
        print_endline ("update: addchild: " ++string_of_int(parentId));
       let parentNode = nodeFromId(parentId); 
       let childNode = nodeFromId(childId);
       parentNode#addChild(childNode);
    }
    | RemoveChild(parentId, childId) => {
        print_endline ("update: removechild: " ++string_of_int(parentId));
       let parentNode = nodeFromId(parentId); 
       let childNode = nodeFromId(childId);
       parentNode#removeChild(childNode);
    }
    | SetText(id, text) => print_endline ("TODO: set text: " ++ text);
    | _ => ()
    };

let update = (v: list(updates)) => {
    print_endline ("Got updates: ");
    Types.showAll(v);
    List.iter(visitUpdate, v);
};


let start = () => {

    let isWorkerReady = ref(false);
    let latestSourceCode: ref(option(Js.t(Js.js_string))) = ref(None);

    let worker = Js_of_ocaml.Worker.create("./worker.js");

    let sendLatestSource = () => {
        switch (latestSourceCode^) {
        | Some(v) => worker##postMessage(Protocol.ToWorker.SourceCodeUpdated(v))
        | None => ();
        };

        latestSourceCode := None;
    };

    let handleMessage = (msg: Protocol.ToRenderer.t) => {
       switch (msg) {
       | Updates(updates) => update(updates)
       | Compiling => {
            isWorkerReady := false;
                           print_endline("Compiling...");
       }
       | Ready => {
           isWorkerReady := true; 
                          print_endline ("Ready!");
            sendLatestSource();
       }
       | _ => ();
       } 
    };

     worker##.onmessage := Js_of_ocaml.Dom_html.handler((evt) => {
         let data = Js.Unsafe.get(evt, "data");
         handleMessage(data);
        Js._true
     });

    let ret = (update) => {
        latestSourceCode := Some(update);
        if (isWorkerReady^) {
            sendLatestSource();   
        }
    };


    let init = app => {

        let win = 
            App.createWindow(app, "Welcome to Revery", ~createOptions={
                ...Window.defaultCreateOptions,
                maximized: true,
            });


        let _projection = Mat4.create();

        let render = () => switch(rootNode^) {
        | None => ()
        | Some(rootNode) => {

            let size = Window.getSize(win);
            let pixelRatio = Window.getDevicePixelRatio(win);
            let scaleFactor = Window.getScaleFactor(win);
            let adjustedHeight = size.height / scaleFactor;
            let adjustedWidth = size.width / scaleFactor;

            rootNode#setStyle(
                Style.make(
                    ~position=LayoutTypes.Relative,
                    ~width=adjustedWidth,
                    ~height=adjustedHeight,
                    (),
                ),
            );

            /* let layoutNode = rootNode#toLayoutNode(~force=false, ()); */
            /* Layout.printCssNode(layoutNode); */

            Layout.layout(~force=true, rootNode);
            rootNode#recalculate();
            rootNode#flushCallbacks();

            Mat4.ortho(
                _projection,
                0.0,
                float_of_int(adjustedWidth),
                float_of_int(adjustedHeight),
                0.0,
                1000.0,
                -1000.0,
            );

            let drawContext = NodeDrawContext.create(~zIndex=0, ~opacity=1.0, ());

            /* Render all geometry that requires an alpha */
            RenderPass.startAlphaPass(
                ~pixelRatio,
                ~scaleFactor,
                ~screenHeight=adjustedHeight,
                ~screenWidth=adjustedWidth,
                ~projection=_projection,
                (),
            );
            rootNode#draw(drawContext);
            RenderPass.endAlphaPass();

            /* (renderFunction^)(); */
        }
        };

        Window.setRenderCallback(win, render);
        Window.setShouldRenderCallback(win, () => true);

        Window.maximize(win);

        /* UI.start(win, render); */
    };

    App.start(init);

    ret;
};



let () = Js.export_all(
  [%js {
      val startRenderer = start;
  }]
);
