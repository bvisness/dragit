# idle thoughts

- The oc_log functions shouldn't require newlines at the end. That's weird.
- WOW iterating over string lists is painful. Wow wow wow.
- Weird that you have to init lists but not string lists.
    - Seems like the init function is there, but not exposed for some reason.
- When splitting a string, it would be nice to know exactly what lifetime the resulting strings are on. My hunch would be that it would obviously not copy the strings, it would just be slicing the original string and therefore not allocating for those - but it needs an arena to allocate the list nodes on. Still, it would be nice to be sure.
    - This is indeed how it works.
- Do our arenas give us zeroed memory or not? It would be really nice for them to give us zeroed memory, please. At least in debug.
- It looks like oc_str8_split doesn't correctly handle consecutive separators - we should get empty strings between them, but we don't.
    - lmao martin explicitly suppressed empty strings. don't do that!!
- It's really hard to work with lists. Really hard. How do I just get the first element of a list? I just want the first string or whatever!
- A crash in wasm code is not easily distinguished from a crash in the runtime. This could probably be made clearer even without a fancy debugging experience.
- It's kinda weird how hard it is to log oc_str8's. And it's weird that the log functions use null-terminated strings...seems like it might be hard for bindings.
- stack smol
- I want to get the mouse position without tracking mouse movement...
- Why does oc_str8_list_collate null-terminate the string? Seems unnecessary, although not really hurting anyone.
- Totally trivial, but "postfix" should be called "suffix" in oc_str8_list_collate.