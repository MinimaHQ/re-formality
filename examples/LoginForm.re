module LoginForm = {
  type value = string;
  type message = string;

  type field =
    | Email
    | Password;

  type state = {
    email: string,
    password: string,
  };

  let get = (field, state) =>
    switch (field) {
    | Email => state.email
    | Password => state.password
    };

  let update = ((field, value), state) =>
    switch (field, value) {
    | (Email, value) => {...state, email: value}
    | (Password, value) => {...state, password: value}
    };

  let valueEmpty = Formality.emptyString;

  module Validators =
    Formality.MakeValidators({
      type t = field;
    });

  type validators =
    Validators.t(Formality.validator(field, value, state, message));

  let validators =
    Formality.(
      Validators.empty
      |> Validators.add(
           Email,
           {
             strategy: Strategy.OnFirstSuccessOrFirstBlur,
             dependents: None,
             validate: (value, _) => {
               let emailRegex = [%bs.re {|/.*@.*\..+/|}];
               switch (value) {
               | "" => Invalid("Email is required")
               | _ when !(emailRegex |> Js.Re.test(value)) =>
                 Invalid("Email is invalid")
               | _ => Valid
               };
             },
           },
         )
      |> Validators.add(
           Password,
           {
             strategy: Strategy.OnFirstBlur,
             dependents: None,
             validate: (value, _) =>
               switch (value) {
               | "" => Invalid("Password is required")
               | _ => Valid
               },
           },
         )
    );
};

module LoginFormContainer = Formality.Make(LoginForm);

let component = ReasonReact.statelessComponent(__MODULE__);

let make = _ => {
  ...component,
  render: _ =>
    <LoginFormContainer
      initialState={email: "", password: ""}
      onSubmit={
        (state, form) => {
          Js.log2("Called with:", state);
          Js.Global.setTimeout(
            () => {
              form.notifyOnSuccess(None);
              Js.Global.setTimeout(form.reset, 3000) |> ignore;
            },
            500,
          )
          |> ignore;
        }
      }>
      ...{
           form =>
             <form
               className="form"
               onSubmit={form.submit |> Formality.Dom.preventDefault}>
               <div className="form-messages-area form-messages-area-lg" />
               <div className="form-content">
                 <h2 className="push-lg"> {"Login" |> ReasonReact.string} </h2>
                 <div className="form-row">
                   <label htmlFor="login--email" className="label-lg">
                     {"Email" |> ReasonReact.string}
                   </label>
                   <input
                     id="login--email"
                     type_="email"
                     value={form.state.email}
                     disabled={form.submitting}
                     onChange={
                       event =>
                         event
                         |> Formality.Dom.toValueOnChange
                         |> form.change(LoginForm.Email)
                     }
                     onBlur={
                       event =>
                         event
                         |> Formality.Dom.toValueOnBlur
                         |> form.blur(LoginForm.Email)
                     }
                   />
                   {
                     switch (LoginForm.Email |> form.results) {
                     | Some(Invalid(message)) =>
                       <div className={Cn.make(["form-message", "failure"])}>
                         {message |> ReasonReact.string}
                       </div>
                     | Some(Valid) =>
                       <div className={Cn.make(["form-message", "success"])}>
                         {{j|✓|j} |> ReasonReact.string}
                       </div>
                     | None => ReasonReact.null
                     }
                   }
                 </div>
                 <div className="form-row">
                   <label htmlFor="login--password" className="label-lg">
                     {"Password" |> ReasonReact.string}
                   </label>
                   <input
                     id="login--password"
                     type_="password"
                     value={form.state.password}
                     disabled={form.submitting}
                     onChange={
                       event =>
                         event
                         |> Formality.Dom.toValueOnChange
                         |> form.change(LoginForm.Password)
                     }
                     onBlur={
                       event =>
                         event
                         |> Formality.Dom.toValueOnBlur
                         |> form.blur(LoginForm.Password)
                     }
                   />
                   {
                     switch (LoginForm.Password |> form.results) {
                     | Some(Invalid(message)) =>
                       <div className={Cn.make(["form-message", "failure"])}>
                         {message |> ReasonReact.string}
                       </div>
                     | Some(Valid) =>
                       <div className={Cn.make(["form-message", "success"])}>
                         {{j|✓|j} |> ReasonReact.string}
                       </div>
                     | None => ReasonReact.null
                     }
                   }
                 </div>
                 <div className="form-row">
                   <button className="push-lg" disabled={form.submitting}>
                     {
                       (form.submitting ? "Submitting..." : "Submit")
                       |> ReasonReact.string
                     }
                   </button>
                   {
                     switch (form.status) {
                     | Formality.FormStatus.Submitted =>
                       <div className={Cn.make(["form-status", "success"])}>
                         {{j|✓ Logged In|j} |> ReasonReact.string}
                       </div>
                     | _ => ReasonReact.null
                     }
                   }
                 </div>
               </div>
             </form>
         }
    </LoginFormContainer>,
};
