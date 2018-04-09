module Validation = Formality__Validation;

module Strategy = Formality__Strategy;

module FormStatus = Formality__FormStatus;

module Utils = Formality__Utils;

module type Config = {
  type field;
  type value;
  type state;
  type message;
  let get: (field, state) => value;
  let update: ((field, value), state) => state;
  let valueEmpty: value => bool;
  let comparator: (field, field) => int;
  type validators;
  let validators: validators;
  module Validators: {
    let find:
      (field, validators) =>
      Validation.asyncValidator(field, value, state, message);
    let fold:
      (
        (field, Validation.asyncValidator(field, value, state, message), 'a) =>
        'a,
        validators,
        'a
      ) =>
      'a;
  };
};

module Make = (Form: Config) => {
  module FieldsSetOrigin =
    Set.Make(
      {
        type t = Form.field;
        let compare = Form.comparator;
      },
    );
  module FieldsSet = {
    type t = FieldsSetOrigin.t;
    let empty = FieldsSetOrigin.empty;
    let isEmpty = FieldsSetOrigin.is_empty;
    let mem = FieldsSetOrigin.mem;
    let add = FieldsSetOrigin.add;
    let remove = FieldsSetOrigin.remove;
  };
  module ResultsMapOrigin =
    Map.Make(
      {
        type t = Form.field;
        let compare = Form.comparator;
      },
    );
  module ResultsMap = {
    type key = ResultsMapOrigin.key;
    type t =
      ResultsMapOrigin.t(option(Validation.validationResult(Form.message)));
    let empty = ResultsMapOrigin.empty;
    let add = ResultsMapOrigin.add;
    let get = (key: key, map: t) =>
      switch (map |> ResultsMapOrigin.find(key)) {
      | result => result
      | exception Not_found => None
      };
  };
  type state = {
    data: Form.state,
    status: FormStatus.t(Form.field, Form.message),
    results: ResultsMap.t,
    validating: FieldsSet.t,
    submittedOnce: bool,
    emittedFields: FieldsSet.t,
  };
  type action =
    | Change((Form.field, Form.value))
    | Blur((Form.field, Form.value))
    | TriggerAsyncValidation(
        Form.field,
        Form.value,
        Validation.validateAsync(Form.value, Form.message),
      )
    | ApplyAsyncResult(
        Form.field,
        Form.value,
        Validation.validationResult(Form.message),
      )
    | Submit
    | SetSubmittedStatus(option(Form.state))
    | SetSubmissionFailedStatus(
        list((Form.field, Form.message)),
        option(Form.message),
      )
    | Reset;
  type interface = {
    state: Form.state,
    status: FormStatus.t(Form.field, Form.message),
    results: Form.field => option(Validation.validationResult(Form.message)),
    validating: Form.field => bool,
    submitting: bool,
    change: (Form.field, Form.value) => unit,
    blur: (Form.field, Form.value) => unit,
    submit: unit => unit,
  };
  let getInitialState = data => {
    data,
    status: FormStatus.Editing,
    results: ResultsMap.empty,
    validating: FieldsSet.empty,
    submittedOnce: false,
    emittedFields: FieldsSet.empty,
  };
  let getValidator = field =>
    switch (Form.validators |> Form.Validators.find(field)) {
    | validator => Some(validator)
    | exception Not_found => None
    };
  let validateDependents = (~data, ~results, ~emittedFields, dependents) =>
    dependents
    |> List.fold_left(
         ((results', emittedFields'), field') => {
           let validator = field' |> getValidator;
           let emitted = emittedFields |> FieldsSet.mem(field');
           switch (validator, emitted) {
           | (None, _)
           | (_, false) => (results', emittedFields')
           | (Some(validator), true) =>
             let value = data |> Form.get(field');
             let result = data |> validator.validate(value);
             (
               results'
               |> ResultsMap.add(
                    field',
                    value |> Form.valueEmpty ? None : Some(result),
                  ),
               emittedFields' |> FieldsSet.add(field'),
             );
           };
         },
         (results, emittedFields),
       );
  let component = "FormalityForm" |> ReasonReact.reducerComponent;
  let make =
      (
        ~initialState: Form.state,
        ~onSubmit:
           (
             Form.state,
             Validation.submissionCallbacks(
               Form.field,
               Form.state,
               Form.message,
             )
           ) =>
           unit,
        children,
      ) => {
    ...component,
    initialState: () => getInitialState(initialState),
    reducer: (action, state) =>
      switch (action) {
      | Change((field, value)) =>
        let data = state.data |> Form.update((field, value));
        switch (field |> getValidator) {
        | None => ReasonReact.Update({...state, data})
        | Some(validator) =>
          let emitted = state.emittedFields |> FieldsSet.mem(field);
          switch (validator.strategy, emitted, state.submittedOnce) {
          | (_, true, _)
          | (_, _, true)
          | (Strategy.OnFirstChange, false, false) =>
            switch (validator.validateAsync) {
            | Some(_validateAsync) =>
              switch (validator.dependents) {
              | Some(dependents) =>
                let (results, emittedFields) =
                  dependents
                  |> validateDependents(
                       ~data,
                       ~results=state.results,
                       ~emittedFields=state.emittedFields,
                     );
                data
                |> validator.validate(value)
                |> Validation.ifResult(
                     ~valid=
                       (_) =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results: results |> ResultsMap.add(field, None),
                           emittedFields,
                         }),
                     ~invalid=
                       result =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results:
                             results |> ResultsMap.add(field, Some(result)),
                           validating:
                             state.validating |> FieldsSet.remove(field),
                           emittedFields:
                             emittedFields |> FieldsSet.add(field),
                         }),
                   );
              | None /* validateAsync: Some -> validator.dependents: None */ =>
                data
                |> validator.validate(value)
                |> Validation.ifResult(
                     ~valid=
                       (_) =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results:
                             state.results |> ResultsMap.add(field, None),
                         }),
                     ~invalid=
                       result =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results:
                             state.results
                             |> ResultsMap.add(field, Some(result)),
                           validating:
                             state.validating |> FieldsSet.remove(field),
                           emittedFields:
                             state.emittedFields |> FieldsSet.add(field),
                         }),
                   )
              }
            | None /* validateAsync: None */ =>
              switch (validator.dependents) {
              | Some(dependents) =>
                let result = data |> validator.validate(value);
                let (results, emittedFields) =
                  dependents
                  |> validateDependents(
                       ~data,
                       ~results=state.results,
                       ~emittedFields=state.emittedFields,
                     );
                ReasonReact.Update({
                  ...state,
                  data,
                  results:
                    results
                    |> ResultsMap.add(
                         field,
                         value |> Form.valueEmpty ? None : Some(result),
                       ),
                  emittedFields: emittedFields |> FieldsSet.add(field),
                });
              | None /* validateAsync: None -> validator.dependents: None */ =>
                let result = data |> validator.validate(value);
                ReasonReact.Update({
                  ...state,
                  data,
                  results:
                    state.results
                    |> ResultsMap.add(
                         field,
                         value |> Form.valueEmpty ? None : Some(result),
                       ),
                  emittedFields: state.emittedFields |> FieldsSet.add(field),
                });
              }
            }
          | (
              Strategy.OnFirstSuccess | Strategy.OnFirstSuccessOrFirstBlur,
              false,
              false,
            ) =>
            switch (validator.validateAsync) {
            | Some(_validateAsync) =>
              switch (validator.dependents) {
              | Some(dependents) =>
                let (results, emittedFields) =
                  dependents
                  |> validateDependents(
                       ~data,
                       ~results=state.results,
                       ~emittedFields=state.emittedFields,
                     );
                data
                |> validator.validate(value)
                |> Validation.ifResult(
                     ~valid=
                       (_) =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results: results |> ResultsMap.add(field, None),
                           emittedFields,
                         }),
                     ~invalid=
                       (_) =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results,
                           emittedFields,
                         }),
                   );
              | None /* validateAsync: Some -> validator.dependents: None */ =>
                data
                |> validator.validate(value)
                |> Validation.ifResult(
                     ~valid=
                       (_) =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results:
                             state.results |> ResultsMap.add(field, None),
                         }),
                     ~invalid=(_) => ReasonReact.Update({...state, data}),
                   )
              }
            | None /* validateAsync: None */ =>
              data
              |> validator.validate(value)
              |> Validation.ifResult(
                   ~valid=
                     result =>
                       switch (validator.dependents) {
                       | Some(dependents) =>
                         let (results, emittedFields) =
                           validateDependents(
                             ~data,
                             ~results=state.results,
                             ~emittedFields=state.emittedFields,
                             dependents,
                           );
                         ReasonReact.Update({
                           ...state,
                           data,
                           results:
                             results
                             |> ResultsMap.add(
                                  field,
                                  value |> Form.valueEmpty ?
                                    None : Some(result),
                                ),
                           emittedFields:
                             emittedFields |> FieldsSet.add(field),
                         });
                       | None =>
                         ReasonReact.Update({
                           ...state,
                           data,
                           results:
                             state.results
                             |> ResultsMap.add(
                                  field,
                                  value |> Form.valueEmpty ?
                                    None : Some(result),
                                ),
                           emittedFields:
                             state.emittedFields |> FieldsSet.add(field),
                         })
                       },
                   ~invalid=
                     (_) =>
                       switch (validator.dependents) {
                       | Some(dependents) =>
                         let (results, emittedFields) =
                           validateDependents(
                             ~data,
                             ~results=state.results,
                             ~emittedFields=state.emittedFields,
                             dependents,
                           );
                         ReasonReact.Update({
                           ...state,
                           data,
                           results,
                           emittedFields,
                         });
                       | None => ReasonReact.Update({...state, data})
                       },
                 )
            }
          | (Strategy.OnFirstBlur | Strategy.OnSubmit, false, false) =>
            ReasonReact.Update({...state, data})
          };
        };
      | Blur((field, value)) =>
        let validator = field |> getValidator;
        let emitted = state.emittedFields |> FieldsSet.mem(field);
        switch (validator, emitted) {
        | (None, _) => ReasonReact.NoUpdate
        | (Some(validator), true) =>
          switch (validator.validateAsync) {
          | Some(validateAsync) =>
            state.data
            |> validator.validate(value)
            |> Validation.ifResult(
                 ~valid=
                   (_) =>
                     ReasonReact.UpdateWithSideEffects(
                       {
                         ...state,
                         results:
                           state.results |> ResultsMap.add(field, None),
                         validating: state.validating |> FieldsSet.add(field),
                         emittedFields:
                           state.emittedFields |> FieldsSet.add(field),
                       },
                       ({send}) =>
                         TriggerAsyncValidation(field, value, validateAsync)
                         |> send,
                     ),
                 ~invalid=
                   result =>
                     ReasonReact.Update({
                       ...state,
                       results:
                         state.results |> ResultsMap.add(field, Some(result)),
                       validating:
                         state.validating |> FieldsSet.remove(field),
                     }),
               )
          | None => ReasonReact.NoUpdate
          }
        | (Some(validator), false) =>
          switch (validator.strategy) {
          | Strategy.OnFirstChange
          | Strategy.OnFirstSuccess
          | Strategy.OnSubmit => ReasonReact.NoUpdate
          | Strategy.OnFirstBlur
          | Strategy.OnFirstSuccessOrFirstBlur =>
            switch (validator.validateAsync) {
            | Some(validateAsync) =>
              state.data
              |> validator.validate(value)
              |> Validation.ifResult(
                   ~valid=
                     (_) =>
                       ReasonReact.UpdateWithSideEffects(
                         {
                           ...state,
                           results:
                             state.results |> ResultsMap.add(field, None),
                           validating:
                             state.validating |> FieldsSet.add(field),
                           emittedFields:
                             state.emittedFields |> FieldsSet.add(field),
                         },
                         ({send}) =>
                           TriggerAsyncValidation(field, value, validateAsync)
                           |> send,
                       ),
                   ~invalid=
                     result =>
                       ReasonReact.Update({
                         ...state,
                         results:
                           state.results
                           |> ResultsMap.add(field, Some(result)),
                         validating:
                           state.validating |> FieldsSet.remove(field),
                         emittedFields:
                           state.emittedFields |> FieldsSet.add(field),
                       }),
                 )
            | None =>
              let result = state.data |> validator.validate(value);
              ReasonReact.Update({
                ...state,
                results:
                  state.results
                  |> ResultsMap.add(
                       field,
                       value |> Form.valueEmpty ? None : Some(result),
                     ),
                emittedFields: state.emittedFields |> FieldsSet.add(field),
              });
            }
          }
        };
      | TriggerAsyncValidation(field, value, validateAsync) =>
        ReasonReact.SideEffects(
          (
            ({send}) =>
              Js.Promise.(
                value
                |> validateAsync
                |> then_(result => {
                     ApplyAsyncResult(field, value, result) |> send;
                     resolve();
                   })
                |> ignore
              )
          ),
        )
      | ApplyAsyncResult(field, value, result) =>
        value === (state.data |> Form.get(field)) ?
          ReasonReact.Update({
            ...state,
            results: state.results |> ResultsMap.add(field, Some(result)),
            validating: state.validating |> FieldsSet.remove(field),
            emittedFields: state.emittedFields |> FieldsSet.add(field),
          }) :
          ReasonReact.NoUpdate
      | Submit =>
        switch (state.validating |> FieldsSet.isEmpty, state.status) {
        | (false, _)
        | (_, FormStatus.Submitting) => ReasonReact.NoUpdate
        | _ =>
          let (valid, results) =
            (true, state.results)
            |> Form.Validators.fold(
                 (field', validator', (valid', results')) => {
                   let value = state.data |> Form.get(field');
                   let currentResultInvalid =
                     switch (results' |> ResultsMap.get(field')) {
                     | Some(Validation.Invalid(_)) => true
                     | Some(Validation.Valid) => false
                     | None => false
                     };
                   let result = state.data |> validator'.validate(value);
                   let results =
                     switch (
                       currentResultInvalid,
                       result,
                       validator'.validateAsync,
                     ) {
                     | (true, Validation.Valid, Some(_)) => results'
                     | (_, Validation.Valid, _) =>
                       results'
                       |> ResultsMap.add(
                            field',
                            value |> Form.valueEmpty ? None : Some(result),
                          )
                     | (_, _, _) =>
                       results' |> ResultsMap.add(field', Some(result))
                     };
                   switch (valid', results |> ResultsMap.get(field')) {
                   | (false, _)
                   | (true, Some(Validation.Invalid(_))) => (false, results)
                   | (true, Some(Validation.Valid))
                   | (_, None) => (true, results)
                   };
                 },
                 Form.validators,
               );
          valid ?
            ReasonReact.UpdateWithSideEffects(
              {
                ...state,
                results,
                status: FormStatus.Submitting,
                submittedOnce: true,
              },
              (
                ({state, send}) =>
                  onSubmit(
                    state.data,
                    {
                      notifyOnSuccess: data =>
                        SetSubmittedStatus(data) |> send,
                      notifyOnFailure: (fieldLevelErrors, serverMessage) =>
                        SetSubmissionFailedStatus(
                          fieldLevelErrors,
                          serverMessage,
                        )
                        |> send,
                      reset: () => Reset |> send,
                    },
                  )
              ),
            ) :
            ReasonReact.Update({
              ...state,
              results,
              status: FormStatus.Editing,
              submittedOnce: true,
            });
        }
      | SetSubmittedStatus(data) =>
        switch (data) {
        | Some(data) =>
          ReasonReact.Update({...state, data, status: FormStatus.Submitted})
        | None => ReasonReact.Update({...state, status: FormStatus.Submitted})
        }
      | SetSubmissionFailedStatus(fieldLevelErrors, serverMessage) =>
        ReasonReact.Update({
          ...state,
          status:
            FormStatus.SubmissionFailed(fieldLevelErrors, serverMessage),
        })
      | Reset => ReasonReact.Update(initialState |> getInitialState)
      },
    render: ({state, send}) =>
      children({
        state: state.data,
        status: state.status,
        results: field => state.results |> ResultsMap.get(field),
        validating: field => state.validating |> FieldsSet.mem(field),
        submitting: state.status === FormStatus.Submitting,
        change: (field, value) => Change((field, value)) |> send,
        blur: (field, value) => Blur((field, value)) |> send,
        submit: (_) => Submit |> send,
      }),
  };
};
